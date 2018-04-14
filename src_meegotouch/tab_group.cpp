#include "tab_group.h"
#include "MTermWidget.h"

#include <QGraphicsAnchorLayout>
#include <QGraphicsWidget>

karin::tab_group::tab_group(QGraphicsItem *parent)
	:MContainer(parent),
	layout(new QGraphicsAnchorLayout),
	current(-1)
{
	setHeaderVisible(false);
	layout -> setContentsMargins(0.0, 0.0, 0.0, 0.0);
	//setFocusProxy(centralWidget());
	centralWidget() -> setLayout(layout);
}

karin::tab_group::~tab_group()
{
}

MTermWidget * karin::tab_group::addTab(qint64 id)
{
	if(id < 0)
		return 0;
	MTermWidget *wid = new MTermWidget(false, centralWidget(), id);
	layout -> addAnchor(wid, Qt::AnchorLeft, layout, Qt::AnchorLeft);
	layout -> addAnchor(wid, Qt::AnchorTop, layout, Qt::AnchorTop);
	layout -> addAnchor(wid, Qt::AnchorBottom, layout, Qt::AnchorBottom);
	layout -> addAnchor(wid, Qt::AnchorRight, layout, Qt::AnchorRight);
	wid -> hide();
	return wid;
}

MTermWidget * karin::tab_group::take(qint64 id)
{
	int i;
	for(i = 0; i < layout -> count(); i++)
		if(dynamic_cast<MTermWidget *>(layout -> itemAt(i)) -> mwId == id)
			break;
	if(i >= 0 && i < layout -> count())
	{
		MTermWidget *wid = dynamic_cast<MTermWidget *>(layout -> itemAt(i));
		wid -> clearFocus();
		wid -> hide();
		layout -> removeAt(i);
		if(wid -> scene())
			wid -> scene() -> removeItem(wid);
		wid -> setParent(0);
		wid -> setParentItem(0);
		return wid;
	}
	return 0;
}

void karin::tab_group::removeTab(qint64 id)
{
	MTermWidget *wid = take(id);
	if(wid)
	{
		wid -> disconnect();
		delete wid;
	}
}

void karin::tab_group::showTab(qint64 id)
{
	if(id >= 0 && current != id)
	{
		if(currentWidget())
		{
			currentWidget() -> clearFocus();
			currentWidget() -> hide();
		}
		current = id;
		if(currentWidget())
		{
			currentWidget() -> show();
			//centralWidget() -> setFocusProxy(currentWidget());
		}
	}
}

MTermWidget * karin::tab_group::currentWidget()
{
	return operator[](current);
}

MTermWidget * karin::tab_group::operator[] (qint64 id)
{
	MTermWidget *wid = 0;
	for(int i = 0; i < layout -> count(); i++)
	{
		wid = dynamic_cast<MTermWidget *>(layout -> itemAt(i));
		if(wid -> mwId == id)
			break;
	}
	return wid;
}

QVector<MTermWidget *> karin::tab_group::getAllTabs()
{
	QVector<MTermWidget *> vec;
	for(int i = 0; i < layout -> count(); i++)
		vec.push_back(dynamic_cast<MTermWidget *>(layout -> itemAt(i)));
	return vec;
}

