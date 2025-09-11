#include "mainwindow.h"

#include <QtWidgets>
#include <cmath>
#include <algorithm>
#include <utility>


namespace {

struct RGB { int r, g, b; };
struct CMYK { double c, m, y, k; };
struct XYZ { double X, Y, Z; };
struct Lab  { double L, a, b; };

const double REF_X = 95.047;
const double REF_Y = 100.0;
const double REF_Z = 108.883;

static inline int clampInt(int v, int lo, int hi){ return std::max(lo, std::min(hi, v)); }


CMYK rgbToCmyk(const RGB &rgb) {
    double k = 1.0 - std::max({rgb.r / 255.0, rgb.g / 255.0, rgb.b / 255.0});
    double c = 0, m = 0, y = 0;
    if (k < 1.0 - 1e-12) {
        c = (1.0 - rgb.r / 255.0 - k) / (1.0 - k);
        m = (1.0 - rgb.g / 255.0 - k) / (1.0 - k);
        y = (1.0 - rgb.b / 255.0 - k) / (1.0 - k);
    } else {
        c = m = y = 0.0;
    }
    return {c,m,y,k};
}


RGB cmykToRgb(const CMYK &cmyk) {
    double r = 255.0 * (1.0 - cmyk.c) * (1.0 - cmyk.k);
    double g = 255.0 * (1.0 - cmyk.m) * (1.0 - cmyk.k);
    double b = 255.0 * (1.0 - cmyk.y) * (1.0 - cmyk.k);
    return { clampInt(int(std::round(r)), 0, 255),
            clampInt(int(std::round(g)), 0, 255),
            clampInt(int(std::round(b)), 0, 255) };
}


static double invGamma(double v) {
    if (v <= 0.04045) return v / 12.92;
    else return std::pow((v + 0.055) / 1.055, 2.4);
}


static double gammaSRGB(double v) {
    if (v <= 0.0031308) return 12.92 * v;
    else return 1.055 * std::pow(v, 1.0/2.4) - 0.055;
}

XYZ rgbToXyz(const RGB &rgb) {
    double r = rgb.r / 255.0;
    double g = rgb.g / 255.0;
    double b = rgb.b / 255.0;

    double rl = invGamma(r);
    double gl = invGamma(g);
    double bl = invGamma(b);

    double X = rl * 0.4124564 + gl * 0.3575761 + bl * 0.1804375;
    double Y = rl * 0.2126729 + gl * 0.7151522 + bl * 0.0721750;
    double Z = rl * 0.0193339 + gl * 0.1191920 + bl * 0.9503041;

    return { X * 100.0, Y * 100.0, Z * 100.0 };
}


std::pair<RGB,bool> xyzToRgb(const XYZ &xyz) {
    double x = xyz.X / 100.0;
    double y = xyz.Y / 100.0;
    double z = xyz.Z / 100.0;

    double rl =  x *  3.2406 + y * (-1.5372) + z * (-0.4986);
    double gl =  x * (-0.9689) + y *  1.8758 + z *  0.0415;
    double bl =  x *  0.0557 + y * (-0.2040) + z *  1.0570;

    bool clipped = false;
    double r = gammaSRGB(rl);
    double g = gammaSRGB(gl);
    double b = gammaSRGB(bl);

    if (r < 0.0 || r > 1.0 || g < 0.0 || g > 1.0 || b < 0.0 || b > 1.0) clipped = true;
    int Ri = clampInt(int(std::round(r * 255.0)), 0, 255);
    int Gi = clampInt(int(std::round(g * 255.0)), 0, 255);
    int Bi = clampInt(int(std::round(b * 255.0)), 0, 255);
    return { {Ri, Gi, Bi}, clipped };
}


Lab xyzToLab(const XYZ &xyz) {
    auto f = [](double t)->double {
        const double thresh = 0.008856;
        if (t > thresh) return std::cbrt(t);
        else return (7.787 * t) + (16.0/116.0);
    };

    double xr = xyz.X / REF_X;
    double yr = xyz.Y / REF_Y;
    double zr = xyz.Z / REF_Z;

    double fx = f(xr);
    double fy = f(yr);
    double fz = f(zr);

    double L = 116.0 * fy - 16.0;
    double a = 500.0 * (fx - fy);
    double b = 200.0 * (fy - fz);

    return {L, a, b};
}

XYZ labToXyz(const Lab &lab) {
    double fy = (lab.L + 16.0) / 116.0;
    double fx = lab.a / 500.0 + fy;
    double fz = fy - lab.b / 200.0;

    auto invf = [](double t)->double {
        const double thresh = 0.008856;
        if (t*t*t > thresh) return t*t*t;
        else return (t - 16.0/116.0) / 7.787;
    };

    double xr = invf(fx);
    double yr = invf(fy);
    double zr = invf(fz);

    return { xr * REF_X, yr * REF_Y, zr * REF_Z };
}

Lab rgbToLab(const RGB &rgb) {
    XYZ xyz = rgbToXyz(rgb);
    return xyzToLab(xyz);
}

std::pair<RGB,bool> labToRgb(const Lab &lab) {
    XYZ xyz = labToXyz(lab);
    return xyzToRgb(xyz);
}

}

// ---------------------- MainWindow implementation ----------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    preview = new QLabel;
    preview->setFixedSize(200, 200);
    preview->setFrameShape(QFrame::Box);

    btnPaletteRGB = new QPushButton("Выбрать цвет (палитра)");

    // левая колонка
    QVBoxLayout *leftVBox = new QVBoxLayout;
    leftVBox->addWidget(new QLabel("Цвет"));
    leftVBox->addWidget(preview, 0, Qt::AlignHCenter);
    leftVBox->addWidget(btnPaletteRGB, 0, Qt::AlignHCenter);
    leftVBox->addStretch();

    auto addRowTo = [](QVBoxLayout *target, const QString &label, QSlider *s, QLineEdit *e){
        QWidget *w = new QWidget;
        QHBoxLayout *h = new QHBoxLayout(w);
        h->addWidget(new QLabel(label));
        h->addWidget(s);
        h->addWidget(e);
        h->setContentsMargins(0,0,0,0);
        target->addWidget(w);
    };

    // ---------- RGB ----------
    QGroupBox *gRGB = new QGroupBox("RGB (0..255)");
    sR = new QSlider(Qt::Horizontal); sR->setRange(0,255);
    sG = new QSlider(Qt::Horizontal); sG->setRange(0,255);
    sB = new QSlider(Qt::Horizontal); sB->setRange(0,255);
    eR = new QLineEdit; eR->setValidator(new QIntValidator(0,255,this));
    eG = new QLineEdit; eG->setValidator(new QIntValidator(0,255,this));
    eB = new QLineEdit; eB->setValidator(new QIntValidator(0,255,this));
    QVBoxLayout *lg = new QVBoxLayout;
    addRowTo(lg,"R",sR,eR);
    addRowTo(lg,"G",sG,eG);
    addRowTo(lg,"B",sB,eB);
    gRGB->setLayout(lg);

    // ---------- LAB ----------
    QGroupBox *gLab = new QGroupBox("LAB (L:0..100, a:-128..127, b:-128..127)");
    sL = new QSlider(Qt::Horizontal); sL->setRange(0,100);
    sa = new QSlider(Qt::Horizontal); sa->setRange(-128,127);
    sb = new QSlider(Qt::Horizontal); sb->setRange(-128,127);
    eL = new QLineEdit; eL->setValidator(new QIntValidator(0,100,this));
    ea = new QLineEdit; ea->setValidator(new QIntValidator(-128,127,this));
    eb = new QLineEdit; eb->setValidator(new QIntValidator(-128,127,this));
    QVBoxLayout *ll = new QVBoxLayout;
    addRowTo(ll,"L",sL,eL);
    addRowTo(ll,"a",sa,ea);
    addRowTo(ll,"b",sb,eb);
    gLab->setLayout(ll);

    // ---------- CMYK ----------
    QGroupBox *gCmyk = new QGroupBox("CMYK (0..100 %)");
    sC = new QSlider(Qt::Horizontal); sC->setRange(0,100);
    sM = new QSlider(Qt::Horizontal); sM->setRange(0,100);
    sY = new QSlider(Qt::Horizontal); sY->setRange(0,100);
    sK = new QSlider(Qt::Horizontal); sK->setRange(0,100);
    eC = new QLineEdit; eC->setValidator(new QIntValidator(0,100,this));
    eM = new QLineEdit; eM->setValidator(new QIntValidator(0,100,this));
    eY = new QLineEdit; eY->setValidator(new QIntValidator(0,100,this));
    eK = new QLineEdit; eK->setValidator(new QIntValidator(0,100,this));
    QVBoxLayout *lc = new QVBoxLayout;
    addRowTo(lc,"C",sC,eC);
    addRowTo(lc,"M",sM,eM);
    addRowTo(lc,"Y",sY,eY);
    addRowTo(lc,"K",sK,eK);
    gCmyk->setLayout(lc);


    QVBoxLayout *rightVBox = new QVBoxLayout;
    rightVBox->addWidget(gRGB);
    rightVBox->addWidget(gLab);
    rightVBox->addWidget(gCmyk);
    rightVBox->addStretch();


    warningLabel = new QLabel;
    warningLabel->setStyleSheet("color: darkred;");
    warningLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout *centerLayout = new QHBoxLayout;
    centerLayout->addLayout(leftVBox);
    centerLayout->addLayout(rightVBox);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(warningLabel);
    mainLayout->addLayout(centerLayout);

    QLabel *authorLabel = new QLabel("Богуш Полина, 1 вариант");
    authorLabel->setAlignment(Qt::AlignCenter);
    authorLabel->setStyleSheet("color: gray; font-style: italic;");
    mainLayout->addWidget(authorLabel);

    setWindowTitle("Color explorer — RGB ↔ LAB ↔ CMYK");
    resize(900, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setFixedSize(width(), height());

    // В конце конструктора MainWindow, после setFixedSize(...)
    QFont fancyFont("Segoe Script", 11);  // рукописный стиль
    setFont(fancyFont);

    // Базовый стиль для всех QSlider
    QString sliderStyle = R"(
    QSlider::groove:horizontal {
        height: 6px;
        background: #d0d0d0;
        border-radius: 3px;
    }
    QSlider::handle:horizontal {
        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                                    stop:0 #66ccff, stop:1 #3399ff);
        border: 1px solid #5c5c5c;
        width: 14px;
        height: 14px;
        margin: -5px 0;
        border-radius: 7px;
    }
    QSlider::handle:horizontal:hover {
        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                                    stop:0 #99ddff, stop:1 #66bbff);
    }
    QSlider::sub-page:horizontal {
        background: #66b3ff;
        border-radius: 3px;
    }
)";
    setStyleSheet(sliderStyle);

    // Для QLabel авторства и предупреждения можно сделать тонкий серый текст
    authorLabel->setStyleSheet("color: gray; font-style: italic; font-size: 11pt;");
    warningLabel->setStyleSheet("color: darkred; font-weight: bold;");





    // ---------- connect signals ----------
    connect(sR, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onRgbSliderChanged(); });
    connect(sG, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onRgbSliderChanged(); });
    connect(sB, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onRgbSliderChanged(); });

    connect(eR, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onRgbEditChanged(); });
    connect(eG, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onRgbEditChanged(); });
    connect(eB, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onRgbEditChanged(); });

    connect(sL, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onLabSliderChanged(); });
    connect(sa, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onLabSliderChanged(); });
    connect(sb, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onLabSliderChanged(); });

    connect(eL, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onLabEditChanged(); });
    connect(ea, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onLabEditChanged(); });
    connect(eb, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onLabEditChanged(); });

    connect(sC, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onCmykSliderChanged(); });
    connect(sM, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onCmykSliderChanged(); });
    connect(sY, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onCmykSliderChanged(); });
    connect(sK, &QSlider::valueChanged, this, [this](int){ if(!internalUpdate) onCmykSliderChanged(); });

    connect(eC, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onCmykEditChanged(); });
    connect(eM, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onCmykEditChanged(); });
    connect(eY, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onCmykEditChanged(); });
    connect(eK, &QLineEdit::editingFinished, this, [this](){ if(!internalUpdate) onCmykEditChanged(); });

    connect(btnPaletteRGB, &QPushButton::clicked, this, &MainWindow::onOpenColorDialog);

    // цвет по умолчанию - белый
    setInternalUpdate(true);
    sR->setValue(255); sG->setValue(255); sB->setValue(255);
    eR->setText("255"); eG->setText("255"); eB->setText("255");
    setInternalUpdate(false);
    setFromRGB(255,255,255);
}

MainWindow::~MainWindow() {}

void MainWindow::onRgbSliderChanged() {
    int r = sR->value(), g = sG->value(), b = sB->value();
    setInternalUpdate(true);
    eR->setText(QString::number(r));
    eG->setText(QString::number(g));
    eB->setText(QString::number(b));
    setInternalUpdate(false);
    setFromRGB(r,g,b);
}

void MainWindow::onRgbEditChanged() {
    int r = eR->text().toInt(), g = eG->text().toInt(), b = eB->text().toInt();
    setInternalUpdate(true);
    sR->setValue(r); sG->setValue(g); sB->setValue(b);
    setInternalUpdate(false);
    setFromRGB(r,g,b);
}

void MainWindow::onLabSliderChanged() {
    double L = sL->value(), a = sa->value(), b = sb->value();
    setInternalUpdate(true);
    eL->setText(QString::number(int(std::round(L))));
    ea->setText(QString::number(int(std::round(a))));
    eb->setText(QString::number(int(std::round(b))));
    setInternalUpdate(false);
    setFromLab(L,a,b);
}

void MainWindow::onLabEditChanged() {
    double L = eL->text().toDouble(), a = ea->text().toDouble(), b = eb->text().toDouble();
    setInternalUpdate(true);
    sL->setValue(int(std::round(L))); sa->setValue(int(std::round(a))); sb->setValue(int(std::round(b)));
    setInternalUpdate(false);
    setFromLab(L,a,b);
}

void MainWindow::onCmykSliderChanged() {
    double c = sC->value()/100.0, m = sM->value()/100.0, y = sY->value()/100.0, k = sK->value()/100.0;
    setInternalUpdate(true);
    eC->setText(QString::number(sC->value()));
    eM->setText(QString::number(sM->value()));
    eY->setText(QString::number(sY->value()));
    eK->setText(QString::number(sK->value()));
    setInternalUpdate(false);
    setFromCmyk(c,m,y,k);
}

void MainWindow::onCmykEditChanged() {
    int c = eC->text().toInt(), m = eM->text().toInt(), y = eY->text().toInt(), k = eK->text().toInt();
    setInternalUpdate(true);
    sC->setValue(c); sM->setValue(m); sY->setValue(y); sK->setValue(k);
    setInternalUpdate(false);
    setFromCmyk(c/100.0, m/100.0, y/100.0, k/100.0);
}

void MainWindow::onOpenColorDialog() {
    QColor col = QColorDialog::getColor(Qt::white, this, "Выберите цвет (sRGB)");
    if (!col.isValid()) return;
    int r = col.red(), g = col.green(), b = col.blue();
    setInternalUpdate(true);
    sR->setValue(r); sG->setValue(g); sB->setValue(b);
    eR->setText(QString::number(r)); eG->setText(QString::number(g)); eB->setText(QString::number(b));
    setInternalUpdate(false);
    setFromRGB(r,g,b);
}


void MainWindow::setFromRGB(int r, int g, int b) {
    // preview
    preview->setStyleSheet(QString("background-color: rgb(%1,%2,%3);").arg(r).arg(g).arg(b));

    RGB rgb{r,g,b};
    CMYK cmyk = rgbToCmyk(rgb);
    Lab lab = rgbToLab(rgb);

    setInternalUpdate(true);
    sC->setValue(int(std::round(cmyk.c * 100.0)));
    sM->setValue(int(std::round(cmyk.m * 100.0)));
    sY->setValue(int(std::round(cmyk.y * 100.0)));
    sK->setValue(int(std::round(cmyk.k * 100.0)));
    eC->setText(QString::number(int(std::round(cmyk.c*100))));
    eM->setText(QString::number(int(std::round(cmyk.m*100))));
    eY->setText(QString::number(int(std::round(cmyk.y*100))));
    eK->setText(QString::number(int(std::round(cmyk.k*100))));

    sL->setValue(int(std::round(lab.L)));
    sa->setValue(int(std::round(lab.a)));
    sb->setValue(int(std::round(lab.b)));
    eL->setText(QString::number(int(std::round(lab.L))));
    ea->setText(QString::number(int(std::round(lab.a))));
    eb->setText(QString::number(int(std::round(lab.b))));
    setInternalUpdate(false);

    warningLabel->clear();
}

void MainWindow::setFromLab(double L, double a_, double b_) {
    Lab lab{L, a_, b_};
    auto [rgb, clipped] = labToRgb(lab);

    preview->setStyleSheet(QString("background-color: rgb(%1,%2,%3);").arg(rgb.r).arg(rgb.g).arg(rgb.b));

    CMYK cmyk = rgbToCmyk(rgb);

    setInternalUpdate(true);

    sR->setValue(rgb.r); sG->setValue(rgb.g); sB->setValue(rgb.b);
    eR->setText(QString::number(rgb.r)); eG->setText(QString::number(rgb.g)); eB->setText(QString::number(rgb.b));

    sC->setValue(int(std::round(cmyk.c * 100.0)));
    sM->setValue(int(std::round(cmyk.m * 100.0)));
    sY->setValue(int(std::round(cmyk.y * 100.0)));
    sK->setValue(int(std::round(cmyk.k * 100.0)));
    eC->setText(QString::number(int(std::round(cmyk.c*100))));
    eM->setText(QString::number(int(std::round(cmyk.m*100))));
    eY->setText(QString::number(int(std::round(cmyk.y*100))));
    eK->setText(QString::number(int(std::round(cmyk.k*100))));
    setInternalUpdate(false);

    if (clipped) {
        warningLabel->setText("Внимание: при преобразовании LAB → RGB некоторые значения вышли за 0..255 — выполнено обрезание.");
    } else {
        warningLabel->clear();
    }
}

void MainWindow::setFromCmyk(double c, double m, double y, double k) {
    CMYK cmyk{c,m,y,k};
    RGB rgb = cmykToRgb(cmyk);
    Lab lab = rgbToLab(rgb);

    preview->setStyleSheet(QString("background-color: rgb(%1,%2,%3);").arg(rgb.r).arg(rgb.g).arg(rgb.b));

    setInternalUpdate(true);

    sR->setValue(rgb.r); sG->setValue(rgb.g); sB->setValue(rgb.b);
    eR->setText(QString::number(rgb.r)); eG->setText(QString::number(rgb.g)); eB->setText(QString::number(rgb.b));

    sL->setValue(int(std::round(lab.L)));
    sa->setValue(int(std::round(lab.a)));
    sb->setValue(int(std::round(lab.b)));
    eL->setText(QString::number(int(std::round(lab.L))));
    ea->setText(QString::number(int(std::round(lab.a))));
    eb->setText(QString::number(int(std::round(lab.b))));
    setInternalUpdate(false);

    warningLabel->clear();
}
