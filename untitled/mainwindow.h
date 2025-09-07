#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QSlider;
class QLineEdit;
class QPushButton;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRgbSliderChanged();
    void onRgbEditChanged();
    void onLabSliderChanged();
    void onLabEditChanged();
    void onCmykSliderChanged();
    void onCmykEditChanged();
    void onOpenColorDialog();

private:
    QWidget *centralWidget;

    // RGB
    QSlider *sR;
    QSlider *sG;
    QSlider *sB;
    QLineEdit *eR;
    QLineEdit *eG;
    QLineEdit *eB;
    QPushButton *btnPaletteRGB;

    // LAB
    QSlider *sL;
    QSlider *sa;
    QSlider *sb;
    QLineEdit *eL;
    QLineEdit *ea;
    QLineEdit *eb;

    // CMYK
    QSlider *sC;
    QSlider *sM;
    QSlider *sY;
    QSlider *sK;
    QLineEdit *eC;
    QLineEdit *eM;
    QLineEdit *eY;
    QLineEdit *eK;

    QLabel *preview;
    QLabel *warningLabel;

    bool internalUpdate = false;
    void setInternalUpdate(bool v) { internalUpdate = v; }


    void setFromRGB(int r, int g, int b);
    void setFromLab(double L, double a, double b);
    void setFromCmyk(double c, double m, double y, double k);
};

#endif // MAINWINDOW_H
