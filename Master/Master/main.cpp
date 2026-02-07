#include <QtWidgets/QApplication>
#include "MainWindow.h"
#include "AppModel.h"
#include "AppController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    AppModel model;
    MainWindow view;
    AppController controller(&model, &view);

    view.show();
    return app.exec();
}
