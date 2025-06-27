#include "async_progress_bar.h"
#include <windows.h>

async_progress_bar::async_progress_bar(QWidget *parent) : QWidget{parent},
                                                          progress_bar{new QProgressBar(this)},
                                                          button{new QPushButton("start", this)},
                                                          button2{new QPushButton("测试", this)}
{
    ui->setupUi(this);
    progress_bar->setStyleSheet(progress_bar_style);
    progress_bar->setRange(0, 1000);

    button->setMinimumSize(100, 50);
    button->setMaximumWidth(100);
    button->setStyleSheet(button_style);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    button2->setMinimumSize(100, 50);
    button2->setMaximumWidth(100);
    button2->setStyleSheet(button_style);
    button2->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(progress_bar);
    layout->addWidget(button, 0, Qt::AlignHCenter);
    layout->addWidget(button2, 0, Qt::AlignHCenter);
    // 设置窗口布局为垂直布局管理器
    this->setLayout(layout);

    setWindowIcon(QIcon(":/resource/images/safe.png"));

    connect(button, &QPushButton::clicked, this, &async_progress_bar::task);
    connect(button2, &QPushButton::clicked, []
            { QMessageBox::information(nullptr, "测试", "没有卡界面！"); });
}

void async_progress_bar::task()
{
    // QMetaObject::invokeMethod 的 lambda 是在主线程运行的
    future = std::async(std::launch::async, [this]
                        {
            QMetaObject::invokeMethod(this, [this] {
                // 这里显示的线程 ID 就是主线程，代表这些任务就是在主线程，即 UI 线程执行
                // QMessageBox::information(nullptr, "线程ID", std::to_string(pthread_self()).c_str());
                QMessageBox::information(nullptr, "线程ID", std::to_string(GetCurrentThreadId()).c_str());
                button->setEnabled(false);
                progress_bar->setRange(0, 1000);
                button->setText("正在执行...");
            });
            for (int i = 0; i <= 1000; ++i) {
                std::this_thread::sleep_for(10ms);
                QMetaObject::invokeMethod(this, [this, i] {
                    progress_bar->setValue(i);
                });
            }
            QMetaObject::invokeMethod(this, [this] {
                button->setText("start");
                button->setEnabled(true);
            });
            // 不在 invokeMethod 中获取线程 ID，这里显示的是子线程的ID
            // auto s = std::to_string(pthread_self());
            auto s = std::to_string(GetCurrentThreadId());
            QMetaObject::invokeMethod(this, [=] {
                QMessageBox::information(nullptr, "线程ID", s.c_str());
            }); });
}

async_progress_bar::~async_progress_bar()
{
    delete ui;
}