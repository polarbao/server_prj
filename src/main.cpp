#include "hard2Ser_2_0.h"
#include <QtWidgets/QApplication>
#include "CLogManager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

	//log
	CLogManager::getInstance()->startLog("./");
	LOG_DEBUG(u8"Ӳ���˷���������");


    hard2Ser_2_0 w;
    w.show();
    return a.exec();
}
