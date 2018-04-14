/*
    This file is part of meego-terminal

    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).

    Contact: Ruslan Mstoi <ruslan.mstoi@nokia.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#include <QApplication>
#include <QGraphicsView>
#include <QInputContext>
#include <QFileInfo>
#include <QDir>
#include <QClipboard>
#include <QGraphicsSceneMouseEvent>
#include <QX11Info>
#include <MInputMethodState>

#include "meditortoolbar.h"
#include "MTerminalDisplay.h"
#include "karin_ut.h"

#include <X11/Xutil.h>

 // if included before Xutil.h Konsole::Screen conflicts with X11 Screen
#include "ScreenWindow.h"

// KeyPress in QEvent::KeyPress will be expanded by preprocessor to 2 because
// of definition of KeyPress in X11 headers, which would result in weird
// compiler errors as "expected unqualified-id before numeric constant". So
// solution from qt_x11_p.h:
// rename a couple of X defines to get rid of name clashes
// resolve the conflict between X11's FocusIn and QEvent::FocusIn
enum {
    XFocusOut = FocusOut,
    XFocusIn = FocusIn,
    XKeyPress = KeyPress,
    XKeyRelease = KeyRelease,
    XNone = None,
    XRevertToParent = RevertToParent,
    XGrayScale = GrayScale,
    XCursorShape = CursorShape
};

#undef FocusOut
#undef FocusIn
#undef KeyPress
#undef KeyRelease
#undef None
#undef RevertToParent
#undef GrayScale
#undef CursorShape

typedef QHash<int, QString> Key2TextHash_t;

/**
 * Used to initialize static hash in keyPressEvent
 */
Key2TextHash_t initKey2TextHash()
{
    Key2TextHash_t key2text;

    key2text[Qt::Key_Tab] = "\t";
    key2text[Qt::Key_Escape] = "\x1b";

    return key2text;
}

Toolbar::Toolbar(QString fileName):
    fileName(fileName)
{
    name = QFileInfo(fileName).baseName();
    name[0] = name[0].toUpper();
    id = MInputMethodState::instance()->registerAttributeExtension(fileName);
}

Toolbar::~Toolbar()
{
    MInputMethodState::instance()->unregisterAttributeExtension(id);
}

const QStringList & Toolbar::getAllToolbars(bool f)
{
	static QStringList m_toolbars;
	static bool hasInit = false;
	if(f)
		hasInit = false;
	if(!hasInit)
	{
		// toolbars are specified in xml files in this directory
		QDir dir(QString(_KARIN_PREFIX_) + IM_TOOLBARS_DIR);
		dir.setFilter(QDir::Readable | QDir::Files);
		dir.setNameFilters(QStringList() << "*.xml");
		m_toolbars.clear();

		foreach(QFileInfo info, dir.entryInfoList()) {
			m_toolbars.append(info.absoluteFilePath());
		}
		hasInit = true;
	}
	return m_toolbars;
}

MTerminalDisplay::MTerminalDisplay(QGraphicsWidget *parent):
    TerminalDisplay(parent),
    m_latchedModifiers(0),
    m_lockedModifiers(0),
    m_activeToolbarIndex(-1),
    m_btnNameCtrl("Ctrl"),
    m_btnNameAlt("Alt"),
    m_copyAction(0),
    m_pasteAction(0),
    m_clearAction(0),
    m_mouseReleasePos(0, 0),
    m_selectionModeEnabled(false),
    m_restoreEditorToolbar(false),
		m_toolbar(0)
{
    m_btnNames << m_btnNameCtrl << m_btnNameAlt;

    m_mod2BtnName[Qt::ControlModifier] = m_btnNameCtrl;
    m_mod2BtnName[Qt::AltModifier] = m_btnNameAlt;

    m_btnName2Mod[m_btnNameCtrl] = Qt::ControlModifier;
    m_btnName2Mod[m_btnNameAlt] = Qt::AltModifier;

    m_copyAction = new QAction(qtTrId("qtn_comm_copy"), this);
    m_copyAction->setVisible(false);
    connect(m_copyAction, SIGNAL(triggered(bool)), SLOT(copyClipboard()));
    // prevent user from copying the same selection again by hiding copy action
    connect(m_copyAction, SIGNAL(triggered(bool)),
            m_copyAction, SLOT(setVisible(bool)));

    m_pasteAction = new QAction(qtTrId("qtn_comm_paste"), this);
    connect(m_pasteAction, SIGNAL(triggered(bool)), SLOT(pasteClipboard()));
    connect(m_pasteAction, SIGNAL(triggered(bool)),
            SLOT(reAppearEditorToolbar()));

    m_clearAction = new QAction(tr("Clear"), this);
    connect(m_clearAction, SIGNAL(triggered(bool)),
            SLOT(clearClipboardContents()));

    onClipboardDataChanged(); // update visibility of copy and clear actions

    connect(QApplication::clipboard(), SIGNAL(dataChanged()),
            SLOT(onClipboardDataChanged()));

    addAction(m_copyAction);
    addAction(m_pasteAction);
    addAction(m_clearAction);
}

MTerminalDisplay::~MTerminalDisplay()
{
    //qDeleteAll(m_toolbars);
		delete m_toolbar;
}

/**
 * [1] Since right now there is no way to read checked state of an input method
 * toolbar button, Control toggle button on the input method toolbar sends key
 * sequence: Ctrl+Shift+Alt+C
 *
 * [2] Currently QKeyEvent generated by the meegotouch input method framework
 * for the IM toolbar buttons do not have text attribute set. For example, text
 * for Tab is set to a string of size 1 with contents '\001', text for Esc is
 * set to a string of size zero. This is different from regular Qt, which sets
 * text of the key events. Since Vt102Emulation expects text to be set, as a
 * workaround it is set here.
 */
void MTerminalDisplay::keyPressEvent( QKeyEvent* event )
{
    static Key2TextHash_t key2text = initKey2TextHash();

    // toggle Ctrl button, see [1]
    if (event->key() == Qt::Key_C &&
        (event->modifiers() ==
         (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier))) {

        updateModifierState(Qt::ControlModifier);
        event->accept();
        return;
    }
    // toggle Alt button, see [1]
    if (event->key() == Qt::Key_A &&
        (event->modifiers() ==
         (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier))) {

        updateModifierState(Qt::AltModifier);
        event->accept();
        return;
    }

    // to get shortcuts with Alt working, set text to key, see [2]
    if (event->modifiers() == Qt::AltModifier) {
        *event = QKeyEvent(event->type(),
                           event->key(),
                           event->modifiers(),
                           QString(event->key()));
    }

    if (key2text.contains(event->key())) { // see [2]
        QString properText = key2text[event->key()];

        if (event->text() != properText) {
            *event = QKeyEvent(event->type(),
                               event->key(),
                               event->modifiers(),
                               properText);
        }
    }

    TerminalDisplay::keyPressEvent(event);
}

/**
 * Takes plain text (which is assumed to be single character representing
 * single key press as provided by the input method) and then controllifies
 * it. That is, returns text that XLookupString returns for that character and
 * control modifier. This is all done to provide the emulation with the proper
 * text() of QKeyEvent when Ctrl toggle button of the input method toolbar is
 * checked.
 *
 */
QString MTerminalDisplay::getCtrlifiedText(const QString& plainText)
{
    QString text;

    // too bad QGraphicsWidget does not have x11Info
    QGraphicsView *focusedGraphicsView =
        dynamic_cast<QGraphicsView *>(QApplication::focusWidget());

    if (!focusedGraphicsView) {
        return plainText;
    }

    QX11Info x11Info = focusedGraphicsView->x11Info();

    // could use XStringToKeysym() to get keysym, but for latin letters
    // keysym == ascii
    KeySym keySym = plainText.toAscii()[0];
    KeyCode keyCode = XKeysymToKeycode(x11Info.display(), keySym);

    if (!keyCode) {
        return plainText;
    }

    XKeyEvent xev; // fake event with the pressed key code and control modifier
    xev.type = XKeyPress;
    xev.serial = 0;
    xev.send_event = False;
    xev.display = x11Info.display();
    xev.window = XNone;
    xev.root = XNone;
    xev.subwindow = XNone;
    xev.time = 0;
    xev.x = 0;
    xev.y = 0;
    xev.x_root = 0;
    xev.y_root = 0;
    xev.state = ControlMask;
    xev.keycode = keyCode;
    xev.same_screen = 1;

    // from QKeyMapperPrivate::translateKeyEventInternal:
    // some XmbLookupString implementations don't return buffer overflow correctly,
    // so we increase the input buffer to allow for long strings...
    // 256 chars * 2 bytes + 1 null-term == 513 bytes
    QByteArray chars;
    chars.resize(513);
    int count = 0;

    count = XLookupString(&xev, chars.data(), chars.size(), &keySym, 0);

    if (!count) {
        return plainText;
    }

    // if stops working consider using mapper as in Qt's translateKeySym
    text = QString::fromLatin1(chars, count);

    return text;
}

void MTerminalDisplay::inputMethodEvent(QInputMethodEvent* event)
{
    // with hwkb on button press it is empty, on release it has the key char
    if (event->commitString().isEmpty()) {
        event->ignore();
        return;
    }
    if (!anyModifierIsActive()) {
        TerminalDisplay::inputMethodEvent(event);
        return;
    }

    QString modText = event->commitString();
    Qt::KeyboardModifiers modifiers = 0;

    if (ctrlModifierIsActive()) {
        modText = getCtrlifiedText(event->commitString());
        // ControlModifier redundant, emulation only uses text
        modifiers |= Qt::ControlModifier;
    }
    if (altModifierIsActive()) {
        modifiers |= Qt::AltModifier;
    }

    // same stuff as TerminalDisplay::inputMethodEvent with modifiers applied
    QKeyEvent keyEvent(QEvent::KeyPress, 0, modifiers, modText);
    emit keyPressedSignal(&keyEvent);

    // Graphics View Framework: this slows down text entry with hw keyboard
    // (cursor movement becomes lazy)
#if 0
    _inputMethodData.preeditString = event->preeditString();
    update(preeditRect() | _inputMethodData.previousPreeditRect);
#endif

    event->accept();
    clearLatchedModifiers();
}

QVariant MTerminalDisplay::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch ((int)query) {

    case M::InputMethodAttributeExtensionIdQuery:
        if (m_toolbar)
            return QVariant(m_toolbar->id);

    default:
        return TerminalDisplay::inputMethodQuery(query);
    }
}

void MTerminalDisplay::setActiveToolbarIndex(int index, bool force)
{
    if (!force && m_activeToolbarIndex == index)
        return;
		delete m_toolbar;
		m_toolbar = 0;
		m_activeToolbarIndex = -1;
		if(index >= 0 && index < Toolbar::getAllToolbars().size())
		{
			//Q_ASSERT(index >= 0 && index < Toolbar::getAllToolbars().count());

			m_activeToolbarIndex = index;

			m_toolbar = new Toolbar(Toolbar::getAllToolbars().at(m_activeToolbarIndex));

			// in case the same toggle modifier button is on multiple toolbars, if its
			// pressed state is changed on one toolbar by the user, that state should
			// be applied in other toolbars too
			foreach(QString name, m_btnNames) {
				toolbarBtnSetPressedExt(name, modifierIsActive(m_btnName2Mod[name]));

				// underline if locked, otherwise un-underline previosuly underlined
				toolbarBtnTextUnderline(name, m_lockedModifiers & m_btnName2Mod[name]);
			}
		}

    QInputContext *inputContext = qApp->inputContext();
    if (!inputContext) {
        return;
    }
    inputContext->update(); // trigger inputMethodQuery
}

void MTerminalDisplay::setNextActiveToolbar()
{
    int i = m_activeToolbarIndex;
    ++i;
    if (i >= Toolbar::getAllToolbars().count()) {
        i = 0;
    }
    setActiveToolbarIndex(i);
}

void MTerminalDisplay::setPrevActiveToolbar()
{
    int i = m_activeToolbarIndex;
    --i;
    if (i < 0) {
        i = Toolbar::getAllToolbars().count() - 1;
    }
    setActiveToolbarIndex(i);
}

int MTerminalDisplay::setActiveToolbar(QString fileName)
{
    fileName = fileName.trimmed();
		const QStringList & tbs = Toolbar::getAllToolbars();

    for (int i = 0; i < tbs.size(); ++i) {
        if (tbs.at(i) == fileName) {
            setActiveToolbarIndex(i);
            break;
        }
    }

    return m_activeToolbarIndex;
}

/**
 * This method tries to implement a workaround for a bug in Maliit Input
 * Method regarding toolbar buttons pressed state. The bug is can be
 * reproduced as:
 *
 * 1) The user pressed a button
 * 2) This method unpresses it
 * 3) The user pressed a button
 * 4) This method unpresses it, but nothing happens
 *
 * The problem with the approach in this method - unnecessary state change
 * if a button is already in given state. For example, if a button is
 * unpressed and this method is called to unpress it, the button will be
 * visibly pressed and unpressed. There is no way to read current button
 * state, so seems to be impossible to solve. Anyway, good thing is that
 * this use case is not needed by this app (the button is pressed if it is
 * unpressed and vice versa).
 */
void MTerminalDisplay::toolbarBtnSetPressedExt(const QString &name,
                                               bool pressed) const
{
    if (pressed) {
        toolbarBtnSetPressed(name, false);
        toolbarBtnSetPressed(name, true);
    }
    else {
        toolbarBtnSetPressed(name, true);
        toolbarBtnSetPressed(name, false);
    }
}

inline
void MTerminalDisplay::toolbarBtnTextUnderline(const QString &name,
                                               bool underline) const
{
    if (!m_toolbar)
        return;

    MInputMethodState::instance()->setExtendedAttribute(
        m_toolbar->id,
        "/toolbar",
        name,
        "text",
        underline ? QString("<u>%1</u>").arg(name) : name);
}

inline
void MTerminalDisplay::toolbarBtnSetPressed(const QString &name,
                                            bool pressed) const
{
    if (!m_toolbar)
        return;

    MInputMethodState::instance()->setExtendedAttribute(
        m_toolbar->id,
        "/toolbar",
        name,
        "pressed",
        pressed ? "true" : "false");
}

inline
bool MTerminalDisplay::modifierIsActive(Qt::KeyboardModifier modifier) const
{
    if (m_latchedModifiers & modifier || m_lockedModifiers & modifier)
        return true;

    return false;
}

void MTerminalDisplay::clearLatchedModifiers()
{
    if (m_latchedModifiers & Qt::ControlModifier) {
        toolbarBtnSetPressedExt(m_btnNameCtrl, false);
    }
    if (m_latchedModifiers & Qt::AltModifier) {
        toolbarBtnSetPressedExt(m_btnNameAlt, false);
    }
    m_latchedModifiers = 0;
}

/**
 * For details, see [1] in the header comment of this class.
 */
void MTerminalDisplay::updateModifierState(Qt::KeyboardModifier modifier)
{
    // clear
    if (m_lockedModifiers & modifier) {
        m_lockedModifiers &= ~modifier;
        toolbarBtnTextUnderline(m_mod2BtnName[modifier], false);
    }

    // latch
    else if (!(m_latchedModifiers & modifier)) {
        m_latchedModifiers |= modifier;
    }

    // lock
    else {
        m_latchedModifiers &= ~modifier;
        m_lockedModifiers |= modifier;

        // user presses button for the second time, it will be unpressed, so
        // press it
        toolbarBtnSetPressedExt(m_mod2BtnName[modifier], true);

        // underline locked button to distinguish from latched
        toolbarBtnTextUnderline(m_mod2BtnName[modifier], true);
    }
}

/**
 * [1] Ctrl + Alt + left mouse button press enable column selection mode in
 * TerminalDisplay.
 */
void MTerminalDisplay::mousePressEvent(QGraphicsSceneMouseEvent *ev)
{
    if (m_selectionModeEnabled) {
        Qt::KeyboardModifiers modifiers = ev->modifiers(); // see [1]

        if (ctrlModifierIsActive()) {
            modifiers |= Qt::ControlModifier;
        }
        if (altModifierIsActive()) {
            modifiers |= Qt::AltModifier;
        }

        if (modifiers != ev->modifiers()) {
            ev->setModifiers(modifiers);
        }

        TerminalDisplay::mousePressEvent(ev);
    }

    hideEditorToolbar();
}

void MTerminalDisplay::mouseReleaseEvent(QGraphicsSceneMouseEvent *ev)
{
    if (m_selectionModeEnabled)
        TerminalDisplay::mouseReleaseEvent(ev);

    m_mouseReleasePos = ev->pos();
    showEditorToolbar();
}

void MTerminalDisplay::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *ev)
{
    if (m_selectionModeEnabled)
        TerminalDisplay::mouseDoubleClickEvent(ev);
}

void MTerminalDisplay::onClipboardDataChanged()
{
    const bool hasText = !QApplication::clipboard()->text().isEmpty();
    m_pasteAction->setVisible(hasText);
    m_clearAction->setVisible(hasText);
}

void MTerminalDisplay::clearClipboardContents()
{
    QApplication::clipboard()->clear();
}

void MTerminalDisplay::focusInEvent(QFocusEvent *event)
{
    onClipboardDataChanged();
    if (m_restoreEditorToolbar)
        showEditorToolbar();
    connect(QApplication::clipboard(), SIGNAL(dataChanged()),
            this, SLOT(onClipboardDataChanged()));
    TerminalDisplay::focusInEvent(event);
}

void MTerminalDisplay::focusOutEvent(QFocusEvent *event)
{
    if (m_editorToolbar && m_editorToolbar->isAppeared()) {
        m_restoreEditorToolbar = true;
        hideEditorToolbar();
    }
    disconnect(QApplication::clipboard(), SIGNAL(dataChanged()),
               this, SLOT(onClipboardDataChanged()));
    TerminalDisplay::focusOutEvent(event);
}

void MTerminalDisplay::showEditorToolbar()
{
    if (!m_editorToolbar) {
        m_editorToolbar.reset(new MEditorToolbar(this));
        m_editorToolbar->setObjectName("MEditorToolbar");

        foreach (QAction *action, actions()) {
            m_editorToolbar->addAction(action);
        }

        connect(m_editorToolbar.data(), SIGNAL(sizeChanged()),
                SLOT(updateEditorToolbarPosition()));
				connect(MInputMethodState::instance(),
						SIGNAL(inputMethodAreaChanged(QRect)),
						this, SLOT(updateEditorToolbarPosition()));

    }

    const QString selectedText = _screenWindow->selectedText(_preserveLineBreaks);
    const QString clipboardText= QApplication::clipboard()->text();
    const bool hasSelectedText = !selectedText.isEmpty();
    const bool selectionChanged =
        hasSelectedText && clipboardText != selectedText;
    const bool showCopyAction = selectionChanged;

    m_copyAction->setVisible(showCopyAction);
    // don't autohide if can copy
    m_editorToolbar->setAutoHideEnabled(!showCopyAction);
    m_editorToolbar->appear();

    updateEditorToolbarPosition();
}

void MTerminalDisplay::hideEditorToolbar()
{
    if (!(m_editorToolbar && m_editorToolbar->isAppeared()))
        return;

    m_editorToolbar->disappear();
}

/**
 * Restarts autohide timer of editor toolbar to prevent auto-hiding toolbar
 * while user is using it.
 *
 * Currently only used with the paste action. Not used with the copy action
 * because if copy action is visible editor toolbar will not auto-hide. Not
 * used with the clear action because if clear is clicked both paste and clear
 * actions will hide.
 */
void MTerminalDisplay::reAppearEditorToolbar()
{
    Q_ASSERT(m_editorToolbar);
    if (!m_copyAction->isVisible()) {
        m_editorToolbar->setAutoHideEnabled(true);
        m_editorToolbar->appear();
    }
}

void MTerminalDisplay::updateEditorToolbarPosition()
{
    qreal posX = m_mouseReleasePos.x();
    qreal posY = m_mouseReleasePos.y();
    // distance safe enough to prevent accidental clicks on the toolbar while
    // double/triple clicking to select text
    qreal safeDistanceY = _fontHeight;
    qreal toolbarHeight = m_editorToolbar->size().height();
    qreal toolbarWidth = m_editorToolbar->size().width();
    qreal myHeight = size().height();
    qreal myWidth = size().width();
    MEditorToolbar::ToolbarPlacement placement =
        MEditorToolbar::AbovePointOfInterest;

    // toolbar fits above the line, show it there
    if (posY >= toolbarHeight + safeDistanceY) {
        placement = MEditorToolbar::AbovePointOfInterest;
				karin::ut * const kut = karin::ut::Instance();
				if(kut -> getSetting<bool>(TRANSLUCENT_INPUTMETHOD) && kut -> getSetting<bool>(ENABLE_VKB))
				{
					qreal kHeight = myHeight;
					QRect imArea = MInputMethodState::instance() -> inputMethodArea();
					if (imArea.x() == 0) { // landscape
						kHeight = imArea.y() - scenePos().y();
					}
					else if (imArea.y() == 0) { // portrait
						kHeight = imArea.x() - scenePos().x();
					}
					posY = qMin(posY, kHeight);
				}
				else
				{
					posY = qMin(posY, myHeight); // make sure always visible at bottom
				}
				posY -= safeDistanceY;
    }

    // toolbar does not fit above the line
    else {
        // toolbar fits below the line, show it there
        if (posY + toolbarHeight + safeDistanceY  <= myHeight) {
            placement = MEditorToolbar::BelowPointOfInterest;
            posY += safeDistanceY;
        }

        // toolbar does not fit below the line, show it just above vkb, this is
        // special case of too little space, with vkb up, landscape & big font
        else {
            placement = MEditorToolbar::AbovePointOfInterest;
            posY = myHeight;

            // since height is small cannot show toolbar at the same X
            // coordinate, because of high risk of accidental clicks of the
            // toolbar, so shown in the oppostite corner
            posX = posX < toolbarWidth ? myWidth : 0;
        }
    }

    m_editorToolbar->setPosition(QPointF(posX, posY), placement);
}

/**
 * [1] To update position of toolbar and hide copy action because selection is
 * cleared during resize (konsole clears selection if resized, see
 * Screen::clearSelection).
 */
void MTerminalDisplay::resizeEvent(QGraphicsSceneResizeEvent * event)
{
    TerminalDisplay::resizeEvent(event);

    if (m_editorToolbar && m_editorToolbar->isAppeared()) {
        showEditorToolbar(); // see [1]
    }
}
