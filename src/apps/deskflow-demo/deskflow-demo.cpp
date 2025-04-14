#include <QApplication>
#include <QMessageBox>
#include <QSysInfo>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  QMessageBox::information(nullptr, "OS name demo", QSysInfo::prettyProductName());
}
