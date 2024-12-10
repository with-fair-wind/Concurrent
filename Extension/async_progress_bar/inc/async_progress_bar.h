#pragma once
#include "ui_async_progress_bar.h"
#include <QMainWindow>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <chrono>
#include <future>
#include <sstream>
#include <string>
#include <thread>
using namespace std::chrono_literals;

// 在 Qt 中，GUI 控件通常只能在创建它们的线程中进行操作，因为它们是线程不安全的
// 可以使用 QMetaObject::invokeMethod 来跨线程调用主线程上的控件方法, 从而在其他线程中安全地更新 UI 控件
class async_progress_bar : public QWidget
{
    Q_OBJECT

public:
    async_progress_bar(QWidget *parent = nullptr);
    ~async_progress_bar();
    void task();

private:
    QString progress_bar_style =
        "QProgressBar {"
        "    border: 2px solid grey;"
        "    border-radius: 5px;"
        "    background-color: lightgrey;"
        "    text-align: center;" // 文本居中
        "    color: #000000;"     // 文本颜色
        "}"
        "QProgressBar::chunk {"
        "    background-color: #7FFF00;"
        "    width: 10px;"     // 设置每个进度块的宽度
        "    font: bold 14px;" // 设置进度条文本字体
        "}";
    QString button_style =
        "QPushButton {"
        "    text-align: center;" // 文本居中
        "}";
    QProgressBar *progress_bar{};
    QPushButton *button{};
    QPushButton *button2{};
    std::future<void> future;
    Ui_async_progress_bar *ui;
};