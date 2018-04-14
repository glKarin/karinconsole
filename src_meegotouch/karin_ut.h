#ifndef _KARIN_UT_H
#define _KARIN_UT_H

#include <QVariant>

#define TYPE_MODE "typeMode"
#define COLOR_SCHEME_SETTING "colorScheme"
#define FULL_SCREEN "fullScreen"
#define ACTIVE_TOOLBAR "activeToolbar"
#define FONT "font"
#define SHOW_BANNER "showInfoBanner"
#define ORIENTATION "orientation"
#define TRANSLUCENT_INPUTMETHOD "translucentInputMethod"
#define ENABLE_VKB "enableVirtualKeyboard"
#define CURSOR_TYPE "terminalCursorType"
#define BLINKING_CURSOR "blinkingCursor"

class QSettings;
class QString;

namespace karin
{
	class ut
	{
		public:
			~ut();
			static ut * Instance();
			void setSetting(const QString &key, const QVariant &value = QVariant());
			QVariant getSetting(const QString &key);
			template <class T> T getSetting(const QString &key);
			template <class T> void setSetting(const QString &key, const T &t);

		private:
			ut();
			QSettings *settings;
			static QVariant defaultSettings(const QString &key);

		private:
			ut(const ut &other);
			ut & operator= (const ut &other);
	};
}

template <class T>
T karin::ut::getSetting(const QString &key)
{
	QVariant var = getSetting(key);
	if(var.isNull() || !var.canConvert<T>())
		return T();
	else
		return var.value<T>();
}

template <class T>
void karin::ut::setSetting(const QString &key, const T &t)
{
	QVariant var(t);
	if(!var.isValid())
		return;
	setSetting(key, var);
}

#endif
