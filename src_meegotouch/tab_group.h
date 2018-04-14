#ifndef _KARIN_TABGROUP_H
#define _KARIN_TABGROUP_H

#include <MContainer>
#include <QVector>

class QGraphicsAnchorLayout;
class MTermWidget;

namespace karin
{
	class terminal;

	class tab_group : public MContainer
	{
		Q_OBJECT

		public:
			tab_group(QGraphicsItem *parent = 0);
			~tab_group();
			MTermWidget * addTab(qint64 id);
			void removeTab(qint64 id);
			MTermWidget * take(qint64 id);
			void showTab(qint64 id);
			qint64 currentTabId()
			{
				return current;
			}
			MTermWidget * currentWidget();
			MTermWidget * operator[] (qint64 id);
			QVector<MTermWidget *> getAllTabs();

		private:
			QGraphicsAnchorLayout *layout;
			qint64 current;
			QMap<qint64, MTermWidget *> tabMap;

			friend class terminal;
			Q_DISABLE_COPY(tab_group)
	};

}

#endif
