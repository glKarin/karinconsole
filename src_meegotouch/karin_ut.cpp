#include "karin_ut.h"

#include <QSettings>
#include <QFont>

#include <QDebug>

#ifdef _KARIN_INSTALL_
#define IM_TOOLBARS_DIR "/usr/share/karin-console/toolbars/"
#else
#define IM_TOOLBARS_DIR "/home/user/karinconsole/src_meegotouch/toolbars/"
#endif


karin::ut::ut()
	:settings(new QSettings)
{
}

karin::ut::~ut()
{
	delete settings;
}

karin::ut * karin::ut::Instance()
{
	static karin::ut s_ut;
	return &s_ut;
}

void karin::ut::setSetting(const QString &key, const QVariant &value)
{
	if(!settings -> contains(key) || value.isNull())
	{
		QVariant defSets = karin::ut::defaultSettings(key);
		if(defSets.isNull())
			return;
		settings -> setValue(key, defSets);
#ifdef _SIMLATOR
		qDebug()<<"Setting key -> \""<<key<<"\" set to default -> "<<defSets;
#endif
	}
	else
		settings -> setValue(key, value);
}

QVariant karin::ut::getSetting(const QString &key)
{
	if(!settings -> contains(key))
		setSetting(key);
	return settings -> value(key);
}

QVariant karin::ut::defaultSettings(const QString &key)
{
	if(key == FONT)
		return QVariant(QVariant(QFont("Monospace", 12).toString()));
	else if(key == COLOR_SCHEME_SETTING)
		return QVariant(1);
	else if(key == FULL_SCREEN)
		return QVariant(false);
	else if(key == ACTIVE_TOOLBAR)
		return QVariant(QVariant(QString("%1%2").arg(IM_TOOLBARS_DIR).arg("karin.xml")));
	else if(key == SHOW_BANNER)
		return QVariant(true);
	else if(key == ORIENTATION)
		return QVariant(0);
	else if(key == TRANSLUCENT_INPUTMETHOD)
		return QVariant(false);
	else if(key == ENABLE_VKB)
		return QVariant(true);
	else if(key == TYPE_MODE)
		return QVariant(0);
	else if(key == CURSOR_TYPE)
		return QVariant(0);
	else if(key == BLINKING_CURSOR)
		return QVariant(false);
	else
		return QVariant();
}

