#include "async_progress_bar.h"

#include <QApplication>
#pragma comment(lib, "user32.lib")

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    auto s = std::to_string(pthread_self());
    QMessageBox::information(nullptr, "主线程ID", s.c_str());

    async_progress_bar w;
    w.show();
    return a.exec();
}