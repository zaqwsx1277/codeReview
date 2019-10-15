// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "scheduler.h"
#include "ui_scheduler.h"
#include "addgpifilterdlg.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QMap>
#include "scheduler/filters/gpifilterloader.h"
#include "find_area.h"
#include "tools/domainfactory.h"
#include "tools/inputtextdlg.h"
#include "renderer/simplerenderer.h"
#include "renderer/fillrenderer.h"
#include "symbols/polygonsymbol.h"
#include "renderer/grouprenderer.h"
#include "symbols/linesymbol.h"
#include "glext/glprogram.h"
#include "createworkareadlg.h"
#include "symbols/texturesymbol.h"
#include "tools/itgimodel.h"
#include "src/tools/mapsource.h"
#include <csfactory.h>
#include "metadata/metadatawidget.h"
#include <QPainter>
#include "sxfdriver/GCSxfDriver.h"
#include <gcmodel.h>
#include <document_manager.h>
#include <documents/writer_document.h>
#include <document_elements/paragraph.h>
#include <styles/paragraph_style.h>
#include <helpers/length.h>
#include <scheduler/datafilterpanel.h>
#include "sheets/SheetDivisionFactory.h"
#include <db.h>
#include <QStandardPaths>
#include <template_master.h>
#include <QTimer>
#include "tools/busydlg.h"
#include "cheackareadlg.h"
#include "ui_workareawidget.h"
#include "select_resource_dlg.h"

#define max(a,b) a<b?b:a
#define min(a,b) a>b?b:a

Scheduler::Scheduler(QWidget* parent) : QMainWindow(parent), ui(new Ui::Scheduler())

{
	ui->setupUi(this);
	connect(ui->m_mapView, SIGNAL(loadFinished()), this, SLOT(postGlInit()));
	connect(ui->addAreaButton, SIGNAL(clicked(bool)), this, SLOT(addArea()));
	connect(ui->editButton, SIGNAL(clicked(bool)), this, SLOT(editArea()));
	connect(ui->clearSelectionButton, SIGNAL(clicked(bool)), this, SLOT(clearSelectionArea()));
	connect(ui->remAreaButton, SIGNAL(clicked(bool)), this, SLOT(remArea()));
	connect(ui->saveAreaButton, SIGNAL(clicked(bool)), this, SLOT(saveArea()));
	connect(ui->cancelAreaButton, SIGNAL(clicked(bool)), this, SLOT(updateAreas()));
	connect(ui->checkButton, SIGNAL(clicked(bool)), this, SLOT(checkAreas()));
	connect(ui->btn_search, SIGNAL(clicked(bool)), this, SLOT(findAreas()));
	connect(ui->btn_finishFormation, SIGNAL(clicked(bool)), this, SLOT(onFinishFormation()));
	connect(ui->btn_selectResource, SIGNAL(clicked(bool)), this, SLOT(onSelectResource()));

	connect(ui->areaView, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), this, SLOT(onAreaSelChanged(QListWidgetItem *)));
	connect(ui->areaView, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onAreaDoubleClick(QListWidgetItem*)));

	ui->splitter->setStretchFactor(0, 0);
	ui->splitter->setStretchFactor(1, 1);
	ui->splitter->setStretchFactor(2, 0);

    ui->gpiBox->setModel(new ItgiModel(false, ui->gpiBox));
	ui->scaleBox->setSheetDivision(SheetDivisionFactory::get(SheetDivisionType::Russian));
	connect(ui->scaleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(scaleChange()));

	sdct = new SheetDivisionControlToolBar(ui->m_mapView, this);
	addToolBar(Qt::ToolBarArea::RightToolBarArea, sdct->getToolBar());
	connect(&sdct->getSelectedSheets(), SIGNAL(changed(const QList<SheetP> &, ObservableCollection::Act)), this, SLOT(onSheetListChanged(const QList<SheetP> &, ObservableCollection::Act)));

	ui->dataWidget->setMapView(ui->m_mapView);

	ui->checkButton->setIcon(QIcon(":/check"));
	ui->applyButton->setIcon(QIcon(":/accept"));
	ui->addAreaButton->setIcon(QIcon(":/add"));
	ui->editButton->setIcon(QIcon(":/edit"));
	ui->clearSelectionButton->setIcon(QIcon(":/unselect"));
	ui->remAreaButton->setIcon(QIcon(":/remsel"));
	ui->imageButton->setIcon(QIcon(":/picture"));
	ui->exportSxfButton->setIcon(QIcon(":/exp"));
	ui->officeButton->setIcon(QIcon(":/export_libreoffice"));
	ui->btn_search->setIcon(QIcon(":filter"));
	ui->btn_finishFormation->setIcon(QIcon(":finish_flag"));
	ui->btn_selectResource->setIcon(QIcon(":following"));
	ui->areaWorkBox->setVisible(false);


	auto rightToolBar = new QToolBar(this);
	addToolBar(Qt::ToolBarArea::RightToolBarArea, rightToolBar);
	QAction *addByAdm = new QAction(QIcon(":/addByAdm1"), tr("Добавить по АТД"), rightToolBar);
	rightToolBar->addAction(addByAdm);
	connect(addByAdm, SIGNAL(triggered(bool)), this, SLOT(addByAdmSlot()));

	QAction *addByGeo = new QAction(QIcon(":/addByAdm"), tr("Добавить по географическому объекту"), rightToolBar);
	rightToolBar->addAction(addByGeo);
	connect(addByGeo, SIGNAL(triggered(bool)), this, SLOT(addByGeoSlot()));

	QAction *addByName = new QAction(QIcon(":/addByName"), tr("Добавить по номенклатуре"), rightToolBar);
	rightToolBar->addAction(addByName);
	connect(addByName, SIGNAL(triggered(bool)), this, SLOT(addByNameSlot()));

    QAction *importFromSXF = new QAction(QIcon(":/sxf"), tr("Добавить из SXF"), rightToolBar);
	rightToolBar->addAction(importFromSXF);
	connect(importFromSXF, SIGNAL(triggered(bool)), this, SLOT(addBySXFSlot()));

	ui->gpiBox->setResizable(true);

	checkDlg = new CheackAreaDlg(this, Qt::WindowStaysOnTopHint);
}

void Scheduler::setConnect()
{
	connect(ui->statusBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onStatusChanged(int)));
	connect(ui->scaleBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onScaleChanged(int)));
	connect(ui->gpiBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onGpiChanged(int)));
	ui->applyButton->setToolTip(tr("Согласовать район работ"));
	connect(ui->applyButton, SIGNAL(clicked(bool)), this, SLOT(applyReject()));
}

Scheduler::~Scheduler()
{
}

void Scheduler::fillBox(vectormodel::GCDomain* dom, QComboBox* box)
{
	for (auto key : dom->keys())
	{
		box->addItem(dom->value(key).toString(), key);
	}
	box->setCurrentIndex(0);
}

MapView * Scheduler::mapView()
{
	return ui->m_mapView;
}

void Scheduler::save()
{
	auto path = QDir::tempPath() + "/schedulerfilters.xml";
	if (ui->dataWidget->filters().size()>0)	ui->dataWidget->save(path);
	else QDir(QDir::tempPath()).remove("schedulerfilters.xml");
}

void Scheduler::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);
	QTimer::singleShot(50, this, SLOT(load()));
}

void Scheduler::addArea()
{
	//InputTextDlg dlg(tr("Наименование района"), tr("Наименование района"));
	CreateWorkAreaDlg dlg(this);
	dlg.fillResources();
	if (!dlg.exec())return;
	QString name = "";//dlg.value();
	WorkArea* wa = dlg.workArea();// new WorkArea(name, , );
	if (!wa)return;
	wa->setParams(ui->gpiBox->gpiId(), ui->scaleBox->currentData().toInt(), ui->statusBox->currentData().toInt());
	addArea(wa);
}

void Scheduler::addArea(WorkArea*wa)
{
	QListWidgetItem* it = new QListWidgetItem(wa->title(), ui->areaView);
	if (!wa->isApproved())
	{
		it->setIcon(QIcon(QPixmap(":error_small")));
		it->setToolTip(tr("Район работ не утвержден!"));
	}
	ui->areaView->addItem(it);
	m_areasMap.insert(it, wa);
	m_areas->addGraphic(wa);

	setItemColor(it, wa);

}
void Scheduler::updateArea(WorkArea*wa)
{
	m_areas->removeGraphic(wa);
	m_areas->addGraphic(wa);
}
void Scheduler::setItemColor(QListWidgetItem* it, WorkArea* wa)
{
	bool app = true;
	bool rej = false;
	for (auto sh : wa->sheets())
	{
		auto st = sh->customData();
		if (st != 1)app = false;
		if (st == 2)rej = true;
	}
	if (rej)
	{
		it->setBackgroundColor(QColor::fromRgb(255, 220, 220));
	}
	else if (app)it->setBackgroundColor(QColor::fromRgb(220, 255, 220));
}

void Scheduler::editArea()
{
	auto item = ui->areaView->currentItem();
	auto wa = m_areasMap[item];
	if (!wa) return;
	CreateWorkAreaDlg dlg(this);
	dlg.fillResources();
	dlg.setWorkArea(wa);
	if (!dlg.exec()) return;
	item->setText(wa->title());
}

void Scheduler::clearSelectionArea()
{
	onAreaSelChanged(nullptr);
}

void Scheduler::remArea()
{
	if (QMessageBox::question(this, tr("Удаление района работ"), tr("Вы действительно хотите удалить район работ?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)return;
	auto items = ui->areaView->selectedItems();
	for (int i = 0; i < items.count(); i++) {
		auto item = items[i];
		if (m_areasMap.contains(item)) {
			auto workarea = m_areasMap.take(item);
			m_deletedAreas.append(workarea);
			//onAreaSelChanged(NULL, NULL);
			m_areas->removeGraphic(workarea);
			m_curArea = nullptr;
		}
		delete item;
		item = nullptr;
	}
	//sdct->drawSelectedArea();
	ui->m_mapView->update();
}

void Scheduler::onAreaSelChanged(QListWidgetItem * cur)
{
	if (m_curArea != nullptr) { // 15.07.2019 ASG NULL != nullptr
		m_areas->removeGraphic(m_curArea);
		if (m_curArea) m_curArea->updateGeometry();
		m_areas->addGraphic(m_curArea);
		m_curArea = nullptr; // 15.07.2019 ASG 0 != nullptr
	}

	ObservableCollection& sh = sdct->getSelectedSheets();
	sh.clear();
	if (cur == NULL) {
		sdct->drawSelectedArea();
		return;
	}
	WorkArea* wa = m_areasMap[cur];
	if (!wa)return;
	wa->loadSheets(ui->m_mapView->getMap()->getCoordinateSystem());
	sh.append(wa->sheets());
	sdct->drawSelectedArea();
	m_curArea = wa;
	ui->btn_finishFormation->setEnabled(!m_curArea->isApproved());
	ui->btn_selectResource->setEnabled(m_curArea->isApproved());
}

void Scheduler::onAreaDoubleClick(QListWidgetItem* item) {
	focus2(m_areasMap[item]);

};

void Scheduler::focus2(WorkArea* wa)
{
	if (!wa)return;
	auto g = wa->geometry();
	if (g)
	{
		GCEnvelope env = g->getEnvelope();
		env = env * 2;
		ui->m_mapView->setExtent(env);
	}
	sdct->drawGridLines();
	ui->m_mapView->update();
}

void Scheduler::scaleChange()
{
	auto sc = ui->scaleBox->currentTerm();
	sdct->setScaleOfSD(sc);
	addByAdmDlg()->setScale(sc);
	addByGeoDlg()->setScale(sc);
	addBySXFDlg()->setScale(sc);
	addByNameDlg()->setScale(sc);
}

void Scheduler::onSheetListChanged(const QList<SheetP>& sheets, ObservableCollection::Act act)
{
	if (!m_curArea)return;
	if (act == ObservableCollection::Add)m_curArea->addSheets(sheets);
	else if (act == ObservableCollection::Delete)
	{
		for(auto sh : sheets)
			m_curArea->removeSheet(sh);
	}
}

void Scheduler::saveArea() // Сохранение района в базу
{
	//checkAreas();
	BusyDlg dlg(this);
	dlg.setProgerss(0);
	dlg.show();
	int count = m_areas->count() + m_deletedAreas.count();
	int c = 0;
	for (auto wa : m_areasMap.values())
	{
		if (wa && !wa->save())
		{
			QMessageBox::warning(this, tr("Ошибка"), wa->error());
			return;
		}
		dlg.setProgerss((c++) * 100 / count);
		dlg.repaint();
		QApplication::processEvents();
	}
	for (auto wa : m_deletedAreas)
	{
		if (!wa->deleteFromDb())
		{
			QMessageBox::warning(this, tr("Ошибка"), wa->error());
			return;
		}
		dlg.setProgerss((c++) * 100 / count);
		dlg.repaint();
		QApplication::processEvents();
	}
	onAreaSelChanged(nullptr);
	QMessageBox::information(this, tr("Сохранение"), tr("Данные успешно сохранены"));
}

void Scheduler::clear()
{
	ui->areaView->clear();
	m_areas->clear();
	m_deletedAreas.clear();
	m_areasMap.clear();
}

void Scheduler::updateAreas(int parent)
{
	clear();
	auto cs = ui->m_mapView->getMap()->getCoordinateSystem();
	auto areas = parent ? WorkArea::loadByParent(parent, cs) 
	: WorkArea::loadAreas(ui->gpiBox->gpiId(), ui->scaleBox->currentScale(), ui->statusBox->currentData().toInt(), cs);
	for (auto area : areas) {
		//area->updateGeometry(ui->m_mapView->getMap()->getCoordinateSystem());
		addArea(area);
	}
	if (ui->areaView->count() > 0)
		ui->areaView->item(0)->setSelected(true);
	ui->m_mapView->update();
}

void Scheduler::onScaleChanged(int index) {
	updateAreas();
};

void Scheduler::onGpiChanged(int index) {
	updateAreas();
}

void Scheduler::onStatusChanged(int index) {
	updateAreas();
}

void Scheduler::applyReject()
{
	auto item = ui->areaView->currentItem();
	auto wa = m_areasMap[item];
	if (!wa)return;
	for (auto sh : wa->sheets())
	{
		if (sh->customData() == 2)wa->removeSheet(sh);
	}
	onAreaSelChanged(item);
}

void Scheduler::on_m_mapView_mapViewClicked(const GCSimplePoint& point)
{
	double x = point.x;
	double y = point.y;
	auto cs = ui->m_mapView->getMap()->getCoordinateSystem();
	auto cs4326 = CSFactory::csByCode(4326);
	cs->project(&x, &y, 1, cs4326);
	GCSimplePoint p(x, y);
	GPIFilter* fil = dynamic_cast<GPIFilter*>(ui->dataWidget->selectedFilter());
	if (!fil)return;
	auto oids = fil->oidsByPoint(p);
	if (!oids.size()) return;
	if (!m_window)
		m_window = new MetadataWidget();
	m_window->show();
	m_window->activateWindow();
	m_window->raise();
	m_window->setMetadataList(oids);
}

void Scheduler::on_exportSxfButton_clicked(bool)
{
	if (!m_curArea) return;
	auto fileName = QFileDialog::getSaveFileName(this, tr("Сохранить"), "", tr("Фильтры (*.sxf)"));
	if (fileName.isEmpty()) return;
	if (!fileName.endsWith(".sxf")) fileName += ".sxf";

	GCSxfDriver* dv = new GCSxfDriver(fileName, "exp.rsc");
	vectormodel::GCModel* model = new vectormodel::GCModel();
	dv->setModel(model);
	auto group = getGroup();
	model->addGroup(group);
	model->setAlias(m_curArea->name());
	auto cs = CSFactory::csByCode(4326);
	auto scales = DomainFactory::getDomain("scales");
	for (auto sh : m_curArea->sheets())
	{
		auto g = sh->toGeom(cs);
		auto f = new GCFeature(group->scheme());
		f->setGeometry(g);
		f->setAttribute("9", sh->getName());
		f->setAttribute("40003", scales->value(sh->getScale()));
		group->addFeature(f);
	}
	dv->save();
}

void Scheduler::on_imageButton_clicked(bool)
{
	auto fileName = QFileDialog::getSaveFileName(this, tr("Сохранить"), "", tr("Фильтры (*.png)"));
	if (fileName.isNull() || fileName.isEmpty()) return;
	if (!fileName.endsWith(".png")) fileName += ".png";
	auto im = ui->m_mapView->grabFramebuffer();
	auto fs = ui->dataWidget->filters();
	int lh = 30;
	QPixmap pix = QPixmap(im.width(), im.height() + fs.size() * lh);
	pix.fill();
	QPainter p(&pix);
	p.drawImage(QPoint(0, 0), im);
	QFont font("Arial", 10);
	p.setFont(font);

	int i = im.height();
	for (auto f : fs)
	{
		p.drawPixmap(5, i + 2, f->pixmap());
		p.drawText(35, i + 20, f->getName());
		i += lh;
	}
	pix.save(fileName);
}

void Scheduler::on_officeButton_clicked(bool) {
	if (ui->areaView->selectedItems().count() != 1)
		return;

	auto cur = ui->areaView->selectedItems().first();
	WorkArea* wa = m_areasMap[cur];

	if (wa->sheets().count() > 0) {
		QString fileName = QFileDialog::getSaveFileName(this, tr("Выберите файл для экспорта"),
			QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
			tr("Open Document (*.odt)"));
		
		if (fileName.isNull() || fileName.isEmpty())
			return;
		
		TemplateMaster tm;
		tm.openTemplate("НЛ по району работ");
		auto wDoc = tm.openFile(tm.files().first());

		QList<QString> sheetNames;
		for (int i = 0; i < wa->sheets().count(); i++)
			sheetNames << wa->sheets()[i]->getName();

		QMap<QString, QVariant> replaceMap;
		replaceMap.insert("#workarea_name", wa->name());
		replaceMap.insert("#itgi_name", ui->gpiBox->currentText());
		replaceMap.insert("#scale", ui->scaleBox->currentText());
		QSqlQuery query(Db::db());
		replaceMap.insert("#worktype", "");
		if (query.exec(
			QString("SELECT value FROM work_types WHERE id = %1")
			.arg(wa->workType())) && query.next()) {
			replaceMap["#worktype"] = query.value(0).toString();
		}

		replaceMap.insert("#deadline", QVariant(wa->toDate()).toString());
		replaceMap.insert("#organization", "");
		if (query.exec(QString("select r.name from accounting.resources r join accounting.organizations o on o.resource_id = r.id WHERE r.id = %1").arg(wa->resource())) && query.next()) {
			replaceMap["#organization"] = query.value(0).toString();
		}

		tm.setData(replaceMap);

		QString currentStr = "";
		for (int i = 0; i < sheetNames.count(); i++) {
			auto paragrath = wDoc->createParagraph();
			QString name = sheetNames.at(i);
			paragrath->setText(name);
			wDoc->appendDocumentElement(paragrath);
		}
		{
			auto paragrath = wDoc->createParagraph();
			QString name = QString("Всего НЛ: %1").arg(sheetNames.count());
			paragrath->setText(name);
			wDoc->appendDocumentElement(paragrath);
		}
		tm.setSaveFileName(fileName);
		tm.save();
		DocumentManager::startWriterDocument(fileName);
	}
	else {
		int n = QMessageBox::warning(this,
			"Ошибка",
			"Выберите район работ для экспорта",
			QMessageBox::Ok
		);
	}
}

void Scheduler::addByAdmSlot()
{
	QDesktopWidget* desktop = QApplication::desktop();
	QPoint p = this->mapToGlobal(QPoint(this->width() - addByAdmDlg()->width(), 150));
	if (addByAdmDlg()->isHidden())addByAdmDlg()->move(p);
	addByAdmDlg()->show();
}

void Scheduler::addBySXFSlot()
{
	QDesktopWidget* desktop = QApplication::desktop();
	QPoint p = this->mapToGlobal(QPoint(this->width() - addBySXFDlg()->width(), 150));
	if (addBySXFDlg()->isHidden())addBySXFDlg()->move(p);
	addBySXFDlg()->show();
}

void Scheduler::addByGeoSlot()
{
	QDesktopWidget* desktop = QApplication::desktop();
	QPoint p = this->mapToGlobal(QPoint(this->width() - addByGeoDlg()->width(), 150));
	if (addByGeoDlg()->isHidden())addByGeoDlg()->move(p);
	addByGeoDlg()->show();
}

void Scheduler::addByNameSlot()
{
	QDesktopWidget* desktop = QApplication::desktop();
	QPoint p = this->mapToGlobal(QPoint(this->width() - addByNameDlg()->width(), 150));
	if (addByNameDlg()->isHidden())addByNameDlg()->move(p);
	addByNameDlg()->show();
}

void Scheduler::onAcceptSXFDlg()
{
	ObservableCollection& sheets = sdct->getSelectedSheets();
	auto addedSheets = addBySXFDlg()->getSheets();
	sheets.append(addedSheets);
	sdct->drawSelectedArea();
}

void Scheduler::onAcceptGeoDlg()
{
	ObservableCollection& sheets = sdct->getSelectedSheets();
	auto addedSheets = addByGeoDlg()->getSheets();
	sheets.append(addedSheets);
	sdct->drawSelectedArea();
}

void Scheduler::onAcceptNameDlg()
{
	ObservableCollection& sheets = sdct->getSelectedSheets();
	auto sh = addByNameDlg()->getSheets();
	sheets.append(sh);
	//	m_model->setSheets(sheets);jhj
	sdct->drawSelectedArea();
}

void Scheduler::onAcceptDlg()
{
	ObservableCollection& sheets = sdct->getSelectedSheets();
	auto addedSheets = addByAdmDlg()->getSheets();
	sheets.append(addedSheets);
	sdct->drawSelectedArea();
}

void Scheduler::load()
{
	auto path = QDir::tempPath() + "/schedulerfilters.xml";
	if (!QFile::exists(path))return;
	if (QMessageBox::question(nullptr, tr("Загрузка слоев"), tr("Загрузить последние открытые слои?\nЗагрузка может занять продолжительное время"),
		QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)return;

	ui->dataWidget->load(path);
}

void Scheduler::checkAreas()
{
	QStringList messages;
	
	auto areas = m_areasMap.values();
	BusyDlg dlg(this);
	dlg.setProgerss(0);
	dlg.show();
	int count = areas.size();
	int c = 0;
	for (auto wa : areas)
	{
		

		wa->loadSheets(ui->m_mapView->getMap()->getCoordinateSystem());
		dlg.setProgerss((100.0*(++c)) / count);
		dlg.repaint();
		QApplication::processEvents();
		//noms.push_back(wa->nomenclatures());
	}
	dlg.hide();
	checkDlg->setMapView(ui->m_mapView);
	checkDlg->setAreas(areas);
	checkDlg->show();
	//for(int i = 0; i < areas.count()-1; i++)
	//{
	//	auto area = areas[i];
	//	for(int j = i+1; j < areas.count(); j++)
	//	{
	//		auto inter = QSet<QString>(noms[i]).intersect(noms[j]);
	//		if (inter.size() > 0)
	//			messages.push_back(tr("Районы %1 и %2 содержат %3 одинаковых листов")
	//				.arg(area->name())
	//				.arg(areas[j]->name())
	//				.arg(inter.size()));
	//	}
	//}
	//if(messages.size())
	//{
	//	QMessageBox::warning(this, tr("Проверка районов"), messages.join('\n'));
	//}
	
}

void Scheduler::findAreas()
{
	FindAreaDialog* dial = new FindAreaDialog(this);
	int res = dial->exec();
	if (res != QDialog::Accepted)
		return;

	QSqlQuery query(Db::db());
	query.prepare(
		" SELECT name, scale_id, gpi_kind_id, org_status_id "
		" FROM workarea "
        " WHERE id = :id"
	);
	query.bindValue(":id", dial->selectedId());
	if (!query.exec() || !query.next())
		return;

	ui->statusBox->setCurrentIndex(ui->statusBox->findData(query.value(3).toULongLong()));
	ui->gpiBox->setCurrentGpi(query.value(2).toULongLong());
	ui->scaleBox->setCurrentIndex(ui->scaleBox->findData(query.value(1).toULongLong()));

	auto text = QString("%1 (").arg(query.value(0).toString());
	auto items = ui->areaView->findItems(text, Qt::MatchStartsWith);
	if (items.count() > 0) {
		items[0]->setSelected(true);
		onAreaSelChanged(items[0]);
        onAreaDoubleClick(items[0]);
	}
}

void Scheduler::onFinishFormation()
{   // Падение (Заявка 216)
    if (!m_curArea) return; // Если не инициализирован район то выход
    m_curArea->approve(); // Утверждение района (пометет как утверждённый и изменённый)
    ui->btn_finishFormation->setEnabled(false); // Блокируем кнопку

    if (ui->areaView->currentItem()) // !! ДОБАВИЛ проверку выделенного айтема перед разименование (Больше падать негде) !!
        ui->areaView->currentItem()->setIcon(QIcon()); // Задаём пустую иконку выделенному элемиенту в списке
    ui->areaView->setToolTip(""); // Сбрасываем подсказку
    ui->btn_selectResource->setEnabled(true); // разбокируем кнопку выбора исполнителя
    // Перевёл и озвучил ASG © 15.07.2019
}

void Scheduler::onSelectResource()
{
	if (!m_curArea) return;
	SelectResourceDlg dlg(m_curArea, this);
	if (!dlg.exec()) return;
	dlg.updateWorkArea();
    auto item = ui->areaView->currentItem();
    if (item != 0)
    {
        item->setText(
                QString("%1 (%2 %3-%4)").arg(m_curArea->name()).arg(dlg.currentName()).arg(m_curArea->fromDate()).arg(m_curArea->toDate())
                );   // AALplan добавлена проверка наличия выделеного района //MVA поменял название ресурса на название района
                     // MVA Исправил отображение текста в РСН планирования
        //данные вроде как на сервре вообще не поступали, отправим
        QSqlQuery query(Db::db());      // AAL для заполнения поля status_id сделан тригер на UPDATE !!!
        if(!query.prepare(
                    "update workarea set res_id = :res_id where id = :workarea_id;"
                ))
        {
            qDebug() << "bad query";
        }

        query.bindValue(":res_id", m_curArea->resource());
        query.bindValue(":workarea_id", m_curArea->id());
        if (!query.exec())
        {
            QMessageBox::critical(this, tr("Ошибка при перезаписи выбранного ресурса"), tr("Невозможно перезаписать в базу данных ваш новый выбор!"));
            qDebug() << query.lastError();
        }
    }
    else
        QMessageBox::warning(this, "Ошибка", "Не выбран район работ", QMessageBox::Ok) ;
}

void Scheduler::addOSM()
{
	auto src = EditorMapSource::inst()->path("OSM");
	osm = new RasterLayer(src);
	IMap* mp = new IMap();


	auto scout = CSFactory::csByCode(102100);
	osm->setCoordinateSystem(scout);
	mp->AddLayer(osm);
	ui->m_mapView->setMap(mp);
}

GCFeatureGroup* Scheduler::getGroup()
{
	GCFeatureScheme sh("20", "");
	sh.addDescription(new GCAttributeDescription("9", "", "", gcString));
	sh.addDescription(new GCAttributeDescription("40003", "", "", gcDouble));
	return new GCFeatureGroup(sh);
}

AddByAdm2Dlg* Scheduler::addByAdmDlg()
{
	if (adm == 0)
	{
		adm = new AddByAdm2Dlg(ui->m_mapView, sdct->getSheetDivision(), this, Qt::WindowStaysOnTopHint);
		connect(adm, SIGNAL(acceptSig()), adm, SLOT(hide()));
		connect(adm, SIGNAL(acceptSig()), this, SLOT(onAcceptDlg()));
		connect(adm, SIGNAL(rejectSig()), adm, SLOT(hide()));
	}
	return adm;
}

AddBySXFDlg* Scheduler::addBySXFDlg()
{
	if (m_sxfDlg == 0)
	{
		m_sxfDlg = new AddBySXFDlg(ui->m_mapView, sdct->getSheetDivision(), this, Qt::WindowStaysOnTopHint);
		connect(m_sxfDlg, SIGNAL(acceptSig()), m_sxfDlg, SLOT(hide()));
		connect(m_sxfDlg, SIGNAL(acceptSig()), this, SLOT(onAcceptSXFDlg()));
		connect(m_sxfDlg, SIGNAL(rejectSig()), m_sxfDlg, SLOT(hide()));
	}
	return m_sxfDlg;
}

AddByAdmDlg* Scheduler::addByGeoDlg()
{
	if (m_geoDlg == 0)
	{
		m_geoDlg = new AddByAdmDlg("geoobjects", false, ui->m_mapView, sdct->getSheetDivision(), this, Qt::WindowStaysOnTopHint);
		connect(m_geoDlg, SIGNAL(acceptSig()), m_geoDlg, SLOT(hide()));
		connect(m_geoDlg, SIGNAL(acceptSig()), this, SLOT(onAcceptGeoDlg()));
		connect(m_geoDlg, SIGNAL(rejectSig()), m_geoDlg, SLOT(hide()));
	}
	return m_geoDlg;
}

AddByNameDlg* Scheduler::addByNameDlg()
{
	if (m_nameDlg == 0)
	{
		m_nameDlg = new AddByNameDlg(ui->m_mapView, sdct->getSheetDivision(), this, Qt::WindowStaysOnTopHint);
		connect(m_nameDlg, SIGNAL(acceptSig()), m_nameDlg, SLOT(hide()));
		connect(m_nameDlg, SIGNAL(acceptSig()), this, SLOT(onAcceptNameDlg()));
		connect(m_nameDlg, SIGNAL(rejectSig()), m_nameDlg, SLOT(hide()));
	}
	return m_nameDlg;
}

void Scheduler::postGlInit()
{
	//addOSM();
	setConnect();
	GroupRenderer* gr = new GroupRenderer();
	gr->AddRenderer(new FillRenderer(new PolygonSymbol(new QColor(128, 128, 128, 100)), GLProgram::getInst("base")));
	gr->AddRenderer(new SimpleRenderer(new LineSymbol(3, new QColor(0, 0, 0, 128)), GLProgram::getInst("base")));
	gr->setBaseScale(100000000000L);
	m_areas = new GraphicsLayer(gr, 0, 255);
	ui->m_mapView->getMap()->AddLayer(m_areas);

	gr = new GroupRenderer();
	QImage im("arearej.png");
	gr->AddRenderer(new FillRenderer(new TextureSymbol(im), GLProgram::getInst("fill")));
	gr->AddRenderer(new SimpleRenderer(new LineSymbol(3, new QColor(250, 0, 0, 128)), GLProgram::getInst("base")));
	gr->setBaseScale(100000000000L);
	m_rej = new GraphicsLayer(gr, 0, 255);
	m_rej->setOpacity(0.5);
	ui->m_mapView->getMap()->AddLayer(m_rej);

	updateAreas();
}
//-----------------------------------------------------------------------------------------
/*!
 * \brief setBtnVisible AAL задаём видимость кнопок (нужно для программы формирования работ)
 * \param inVisible статус видимости кнопок
 */
void Scheduler::setBtnVisible (bool inVisible)
{
    ui -> checkButton -> setVisible (inVisible);
    ui -> applyButton -> setVisible (inVisible);
    ui -> addAreaButton -> setVisible (inVisible);
    ui -> editButton -> setVisible (inVisible);
    ui -> exportSxfButton -> setVisible (inVisible);
}
//-----------------------------------------------------------------------------------------
