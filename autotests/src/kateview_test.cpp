/* This file is part of the KDE libraries
   Copyright (C) 2010 Milian Wolff <mail@milianw.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kateview_test.h"
#include "moc_kateview_test.cpp"

#include <katedocument.h>
#include <kateview.h>
#include <ktexteditor/movingcursor.h>
#include <kateconfig.h>
#include <katebuffer.h>

#include <QtTestWidgets>
#include <QTemporaryFile>

using namespace KTextEditor;

QTEST_MAIN(KateViewTest)

KateViewTest::KateViewTest()
    : QObject()
{
}

KateViewTest::~KateViewTest()
{
}

void KateViewTest::testCoordinatesToCursor()
{
    KTextEditor::DocumentPrivate doc(false, false);
    doc.setText("Hi World!\nHi\n");

    KTextEditor::View* view1 = static_cast<KTextEditor::View*>(doc.createView(Q_NULLPTR));
    view1->show();

    QCOMPARE(view1->coordinatesToCursor(view1->cursorToCoordinate(KTextEditor::Cursor(0, 2))),
             KTextEditor::Cursor(0, 2));
    QCOMPARE(view1->coordinatesToCursor(view1->cursorToCoordinate(KTextEditor::Cursor(1, 1))),
             KTextEditor::Cursor(1, 1));
    // behind end of line should give an invalid cursor
    QCOMPARE(view1->coordinatesToCursor(view1->cursorToCoordinate(KTextEditor::Cursor(1, 5))),
             KTextEditor::Cursor::invalid());
    QCOMPARE(view1->cursorToCoordinate(KTextEditor::Cursor(3, 1)), QPoint(-1, -1));
}

void KateViewTest::testCursorToCoordinates()
{
    KTextEditor::DocumentPrivate doc(false, false);
    doc.setText("int a;");

    KTextEditor::ViewPrivate *view = new KTextEditor::ViewPrivate(&doc, 0);
    view->config()->setDynWordWrap(true);
    view->show();

    // don't crash, see https://bugs.kde.org/show_bug.cgi?id=337863
    view->cursorToCoordinate(Cursor(0, 0));
    view->cursorToCoordinate(Cursor(1, 0));
    view->cursorToCoordinate(Cursor(-1, 0));
}

void KateViewTest::testReloadMultipleViews()
{
    QTemporaryFile file("XXXXXX.cpp");
    file.open();
    QTextStream stream(&file);
    const QString line = "const char* foo = \"asdf\"\n";
    for (int i = 0; i < 200; ++i) {
        stream << line;
    }
    stream << flush;
    file.close();

    KTextEditor::DocumentPrivate doc;
    QVERIFY(doc.openUrl(QUrl::fromLocalFile(file.fileName())));
    QCOMPARE(doc.highlightingMode(), QString("C++"));

    KTextEditor::ViewPrivate *view1 = new KTextEditor::ViewPrivate(&doc, 0);
    KTextEditor::ViewPrivate *view2 = new KTextEditor::ViewPrivate(&doc, 0);
    view1->show();
    view2->show();
    QCOMPARE(doc.views().count(), 2);

    QVERIFY(doc.documentReload());
}

void KateViewTest::testLowerCaseBlockSelection()
{
    // testcase for https://bugs.kde.org/show_bug.cgi?id=258480
    KTextEditor::DocumentPrivate doc;
    doc.setText("nY\nnYY\n");

    KTextEditor::ViewPrivate *view1 = new KTextEditor::ViewPrivate(&doc, 0);
    view1->setBlockSelection(true);
    view1->setSelection(Range(0, 1, 1, 3));
    view1->lowercase();

    QCOMPARE(doc.text(), QString("ny\nnyy\n"));
}

void KateViewTest::testSelection()
{
    // see also: https://bugs.kde.org/show_bug.cgi?id=277422
    // wrong behavior before:
    // Open file with text
    // click at end of some line (A) and drag to right, i.e. without selecting anything
    // click somewhere else (B)
    // shift click to another place (C)
    // => expected: selection from B to C
    // => actual: selection from A to C

    QTemporaryFile file("XXXXXX.txt");
    file.open();
    QTextStream stream(&file);
    stream << "A\n"
           << "B\n"
           << "C";
    stream << flush;
    file.close();

    KTextEditor::DocumentPrivate doc;
    QVERIFY(doc.openUrl(QUrl::fromLocalFile(file.fileName())));

    KTextEditor::ViewPrivate *view = new KTextEditor::ViewPrivate(&doc, 0);
    view->resize(100, 100);
    view->show();

    // hackish but works: access to KateViewInternal
    QObject *internalView = view->childAt(50, 50);
    QCOMPARE(internalView->metaObject()->className(), "KateViewInternal");

    const QPoint afterA = view->cursorToCoordinate(Cursor(0, 1));
    const QPoint afterB = view->cursorToCoordinate(Cursor(1, 1));
    const QPoint afterC = view->cursorToCoordinate(Cursor(2, 1));

    // click after A
    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonPress, afterA,
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier));

    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonRelease, afterA,
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier));
    QCOMPARE(view->cursorPosition(), Cursor(0, 1));
    // drag to right
    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonPress, afterA,
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier));

    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseMove, afterA + QPoint(50, 0),
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier));

    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonRelease, afterA + QPoint(50, 0),
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier));

    QCOMPARE(view->cursorPosition(), Cursor(0, 1));
    QVERIFY(!view->selection());

    // click after C
    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonPress, afterC,
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier));

    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonRelease, afterC,
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier));

    QCOMPARE(view->cursorPosition(), Cursor(2, 1));
    // shift+click after B
    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonPress, afterB,
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::ShiftModifier));

    QCoreApplication::sendEvent(internalView, new QMouseEvent(QEvent::MouseButtonRelease, afterB,
                                Qt::LeftButton, Qt::LeftButton,
                                Qt::ShiftModifier));

    QCOMPARE(view->cursorPosition(), Cursor(1, 1));
    QCOMPARE(view->selectionRange(), Range(1, 1, 2, 1));
}

void KateViewTest::testKillline()
{
    KTextEditor::DocumentPrivate doc;
    doc.insertLines(0, QStringList()
        << "foo"
        << "bar"
        << "baz"
    );

    KTextEditor::ViewPrivate *view = new KTextEditor::ViewPrivate(&doc, 0);

    view->setCursorPositionInternal(KTextEditor::Cursor(1, 2));
    view->killLine();

    QCOMPARE(doc.text(), QLatin1String("foo\nbaz\n"));

    doc.clear();
    QVERIFY(doc.isEmpty());

    doc.insertLines(0, QStringList()
        << "foo"
        << "bar"
        << "baz"
        << "xxx"
    );

    view->setCursorPositionInternal(KTextEditor::Cursor(1, 2));
    view->shiftDown();
    view->killLine();

    QCOMPARE(doc.text(), QLatin1String("foo\nxxx\n"));
}

// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
