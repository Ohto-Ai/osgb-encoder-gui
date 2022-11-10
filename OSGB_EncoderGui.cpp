#include "OSGB_EncoderGui.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFuture>
#include <QtConcurrent>
#include <QClipboard>
#include <QMessageBox>

#include "OSGB_Encoder.hpp"

OSGB_EncoderGui::OSGB_EncoderGui(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	
	connect(ui.importButton, &QPushButton::clicked, this, &OSGB_EncoderGui::onImportButtonClicked);
	connect(ui.randomKeyButton, &QPushButton::clicked, this, &OSGB_EncoderGui::onRandomKeyButtonClicked);
	connect(ui.copyRandomKeyButton, &QPushButton::clicked, this, &OSGB_EncoderGui::onCopyRandomKeyButtonClicked);
	connect(ui.encodeButton, &QPushButton::clicked, this, &OSGB_EncoderGui::onEncodeButtonClicked);
	connect(ui.decodeButton, &QPushButton::clicked, this, &OSGB_EncoderGui::onDecodeButtonClicked);
}

OSGB_EncoderGui::~OSGB_EncoderGui()
{}

bool OSGB_EncoderGui::qCopyDirectory(QDir fromDir, QDir toDir, bool bCoverIfFileExists)
{
	if (!toDir.exists())
	{
		if (!toDir.mkdir(toDir.absolutePath()))
			return false;
	}

	QFileInfoList fileInfoList = fromDir.entryInfoList();
	foreach(QFileInfo fileInfo, fileInfoList)
	{
		if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
			continue;

		//拷贝子目录
		if (fileInfo.isDir())
		{
			//递归调用拷贝
			if (!qCopyDirectory(fileInfo.filePath(), toDir.filePath(fileInfo.fileName()), bCoverIfFileExists))
				return false;
		}
		//拷贝子文件
		else
		{
			if (bCoverIfFileExists && toDir.exists(fileInfo.fileName()))
			{
				toDir.remove(fileInfo.fileName());
			}
			if (!QFile::copy(fileInfo.filePath(), toDir.filePath(fileInfo.fileName())))
			{
				return false;
			}
			qApp->processEvents();
		}
	}
	return true;
}

QStringList OSGB_EncoderGui::fetchFileList(QString path, QStringList nameFilters)
{
	// 递归查找所有json文件
	QDir dir(path);
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::NoSymLinks);
	for (auto& file : files)
	{
		file = path + "/" + file;
	}
	QStringList dirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (int i = 0; i < dirs.size(); ++i)
	{
		files << fetchFileList(path + "/" + dirs[i], nameFilters);
	}
	return files;
}

void OSGB_EncoderGui::onRandomKeyButtonClicked()
{
	// 随机生成key
	QByteArray key = QCryptographicHash::hash(QByteArray::number(QDateTime::currentMSecsSinceEpoch()), QCryptographicHash::Md5);
	ui.encodeKeyEdit->setText(key.toHex());
}

void OSGB_EncoderGui::onCopyRandomKeyButtonClicked()
{
	// 复制key
	QClipboard* clipboard = QApplication::clipboard();
	clipboard->setText(ui.encodeKeyEdit->text());
	ui.copyRandomKeyButton->setText("Copied!");
	QTimer::singleShot(1000, this, [=]() {
		ui.copyRandomKeyButton->setText("Copy Encode Key");
		});
}

void OSGB_EncoderGui::onImportButtonClicked()
{
	auto rootJsonPath = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Json Files (*.json)"));
	ui.rootNodePath->setText(rootJsonPath);

	auto jsonDirPath = QFileInfo(rootJsonPath).path();

	ui.importButton->setText("Import...");
	ui.importButton->setEnabled(false);
	ui.encodeButton->setEnabled(false);

	QFuture<void> future = QtConcurrent::run(
		[this, jsonDirPath] {
			auto jsonFiles = fetchFileList(jsonDirPath, { "*.json" });
			auto b3dmFiles = fetchFileList(jsonDirPath, { "*.b3dm" });

			ui.labelJsonCount->setText(QString{ "json count: %1" }.arg(jsonFiles.size()));
			ui.labelB3dmCount->setText(QString{ "b3dm count: %1" }.arg(b3dmFiles.size()));
		});
	
	QFutureWatcher<void>* watcher = new QFutureWatcher<void>(this);
	connect(watcher, &QFutureWatcher<void>::finished, this, [=]() {
		ui.importButton->setText("Import");
		ui.importButton->setEnabled(true);
		ui.encodeButton->setEnabled(true);
		watcher->deleteLater();
		});
	watcher->setFuture(future);
}

void OSGB_EncoderGui::onEncodeButtonClicked()
{
	auto rootJsonPath = ui.rootNodePath->text();
	auto srcPath = QFileInfo(rootJsonPath).path();
	auto encodeKey = ui.encodeKeyEdit->text();
	auto exportPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (QDir(exportPath) != QDir(srcPath))
	{
		exportPath += "/" + QFileInfo(srcPath).fileName() + "_encrypted";
		if (QMessageBox::Cancel == QMessageBox::warning(this, "Warning", QString("Export to %1").arg(exportPath), QMessageBox::Ok | QMessageBox::Cancel))
			return;
	}
	else
	{
		if (QMessageBox::Cancel == QMessageBox::warning(this, "Warning", "The export path is the same as the source path, will overwrite?", QMessageBox::Ok | QMessageBox::Cancel))
			return;
	}

	ui.encodeButton->setText("Export...");
	ui.encodeButton->setEnabled(false);
	ui.decodeButton->setEnabled(false);
	ui.importButton->setEnabled(false);
	ui.encodeKeyEdit->setEnabled(false);
	ui.randomKeyButton->setEnabled(false);
	ui.progressBar->setValue(0);
	if (QDir(exportPath) != QDir(srcPath))
	{
		qCopyDirectory(QDir(srcPath), QDir(exportPath), true);
		ui.statusLabel->setText("File Copied...");
	}

	qApp->processEvents();
	
	QStringList jsonFiles = fetchFileList(exportPath, { "*.json" });
	OSGB_Encoder encoder;
	auto xorKey = QCryptographicHash::hash(encodeKey.toLatin1(), QCryptographicHash::Md5).toHex();
	QList<double> diffKey{};
	for(auto c: xorKey)
	{
		auto i = static_cast<int> (c);
		diffKey.push_back(static_cast<double>(i * i));
	}

	// 遍历jsonFiles， 并更新processBar进度
	for (int i = 0; i < jsonFiles.size(); ++i)
	{
		ui.statusLabel->setText(QString{ "Encode %1" }.arg(QFileInfo(jsonFiles[i]).fileName()));
		encoder.encode(jsonFiles[i], diffKey);
		ui.progressBar->setValue((i + 1) * 100 / jsonFiles.size());
		qApp->processEvents();
	}

	ui.statusLabel->setText("Done");
	ui.randomKeyButton->setEnabled(true);
	ui.encodeKeyEdit->setEnabled(true);
	ui.encodeButton->setEnabled(true);
	ui.importButton->setEnabled(true);
	ui.decodeButton->setEnabled(true);
	ui.encodeButton->setText("Encode");
}

void OSGB_EncoderGui::onDecodeButtonClicked()
{
	auto rootJsonPath = ui.rootNodePath->text();
	auto srcPath = QFileInfo(rootJsonPath).path();
	auto encodeKey = ui.encodeKeyEdit->text();
	auto exportPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (QDir(exportPath) != QDir(srcPath))
	{
		exportPath += "/" + QFileInfo(srcPath).fileName() + "_noencrypted";
		if (QMessageBox::Cancel == QMessageBox::warning(this, "Warning", QString("Export to %1").arg(exportPath), QMessageBox::Ok | QMessageBox::Cancel))
			return;
	}
	else
	{
		if (QMessageBox::Cancel == QMessageBox::warning(this, "Warning", "The export path is the same as the source path, will overwrite?", QMessageBox::Ok | QMessageBox::Cancel))
			return;
	}

	ui.decodeButton->setText("Export...");
	ui.decodeButton->setEnabled(false);
	ui.encodeButton->setEnabled(false);
	ui.importButton->setEnabled(false);
	ui.encodeKeyEdit->setEnabled(false);
	ui.randomKeyButton->setEnabled(false);
	ui.progressBar->setValue(0);
	if (QDir(exportPath) != QDir(srcPath))
	{
		qCopyDirectory(QDir(srcPath), QDir(exportPath), true);
		ui.statusLabel->setText("File Copied...");
	}

	qApp->processEvents();

	QStringList jsonFiles = fetchFileList(exportPath, { "*.json" });
	OSGB_Encoder encoder;
	auto xorKey = QCryptographicHash::hash(encodeKey.toLatin1(), QCryptographicHash::Md5).toHex();
	QList<double> diffKey{};
	for(auto c: xorKey)
	{
		auto i = static_cast<int> (c);
		diffKey.push_back(static_cast<double>(i * i));
	}

	// 遍历jsonFiles， 并更新processBar进度
	for (int i = 0; i < jsonFiles.size(); ++i)
	{
		ui.statusLabel->setText(QString{ "Decode %1" }.arg(QFileInfo(jsonFiles[i]).fileName()));
		encoder.decode(jsonFiles[i], diffKey);
		ui.progressBar->setValue((i + 1) * 100 / jsonFiles.size());
		qApp->processEvents();
	}

	ui.statusLabel->setText("Done");
	ui.randomKeyButton->setEnabled(true);
	ui.encodeKeyEdit->setEnabled(true);
	ui.encodeButton->setEnabled(true);
	ui.decodeButton->setEnabled(true);
	ui.importButton->setEnabled(true);
	ui.decodeButton->setText("Decode");
}
