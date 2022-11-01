#pragma once

#include <QtWidgets/QMainWindow>
#include <QDir>
#include "ui_OSGB_EncoderGui.h"

class OSGB_EncoderGui : public QMainWindow
{
    Q_OBJECT

public:
    OSGB_EncoderGui(QWidget *parent = nullptr);
    ~OSGB_EncoderGui();

    static bool qCopyDirectory(QDir fromDir, QDir toDir, bool bCoverIfFileExists);
    static QStringList fetchFileList(QString path, QStringList nameFilters);

    void onRandomKeyButtonClicked();
	void onCopyRandomKeyButtonClicked();
    void onImportButtonClicked();
    void onExportButtonClicked();

private:
    Ui::OSGB_EncoderGuiClass ui;
};
