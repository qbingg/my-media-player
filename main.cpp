#include "mainwindow.h"

#include <QApplication>

QString obsStyleSheet = R"(

/* Sliders */

QSlider::groove {
    background-color: #BABABA;
    border: none;
    border-radius: 2px;
}

QSlider::groove:horizontal {
    height: 4px;
}

QSlider::groove:vertical {
    width: 4px;
}

QSlider::sub-page:horizontal {
    background-color: #476bd7;
    border-radius: 2px;
}

QSlider::sub-page:horizontal:disabled {
    background-color: #BABABA;
    border-radius: 2px;
}

QSlider::add-page:horizontal:disabled {
    background-color: #1d1f26;
    border-radius: 2px;
}

QSlider::add-page:vertical {
    background-color: #476bd7;
    border-radius: 2px;
}

QSlider::add-page:vertical:disabled {
    background-color: #BABABA;
    border-radius: 2px;
}

QSlider::sub-page:vertical:disabled {
    background-color: #1d1f26;
    border-radius: 2px;
}

QSlider::handle {
    background-color: #ffffff;
    border-radius: 4px;
/***************白色背景：为滑块添加描边aa****************/
    border: 1px solid #808080;
}

QSlider::handle:horizontal {
    height: 10px;
    width: 20px;
    /* Handle is placed by default on the contents rect of the groove. Expand outside the groove */
    margin: -3px 0;
}

QSlider::handle:vertical {
    width: 10px;
    height: 20px;
    /* Handle is placed by default on the contents rect of the groove. Expand outside the groove */
    margin: 0 -3px;
}

QSlider::handle:hover {
    background-color: #c2c2c2;
}

QSlider::handle:pressed {
    background-color: #d6d6d6;
}

QSlider::handle:disabled {
    background-color: #adadad;
}

)";

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyleSheet(obsStyleSheet);
    MainWindow w;
    w.show();
    return a.exec();
}
