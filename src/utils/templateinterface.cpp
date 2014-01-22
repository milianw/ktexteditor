/* This file is part of the KDE libraries
  Copyright (C) 2004, 2010 Joseph Wenninger <jowenn@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "templateinterface.h"
#include "document.h"
#include "view.h"
#include <QString>
#include <QDate>
#include <QRegExp>
#include <KMessageBox>
#include <KLibrary>
#include <KLocalizedString>
#include <QLocale>
#include <QHostInfo>

#define DUMMY_VALUE QLatin1String("!KTE:TEMPLATEHANDLER_DUMMY_VALUE!")

using namespace KTextEditor;

bool TemplateInterface::expandMacros(QMap<QString, QString> &map, QWidget *parentWindow)
{
    QDateTime datetime = QDateTime::currentDateTime();
    QDate date = datetime.date();
    QTime time = datetime.time();
    typedef QString(*kabcbridgecalltype)(const QString &, QWidget *, bool * ok);
    kabcbridgecalltype kabcbridgecall = 0;

    QStringList kabcitems;
    kabcitems << QLatin1String("firstname")
              << QLatin1String("lastname")
              << QLatin1String("fullname")
              << QLatin1String("email");

    QMap<QString, QString>::Iterator it;
    for (it = map.begin(); it != map.end(); ++it) {
        QString placeholder = it.key();
        if (map[ placeholder ].isEmpty()) {
            if (placeholder == QLatin1String("index")) {
                map[ placeholder ] = QLatin1String("i");
            } else if (placeholder == QLatin1String("loginname")) {
            } else if (kabcitems.contains(placeholder)) {
                if (kabcbridgecall == 0) {
                    KLibrary lib(QLatin1String("ktexteditorkabcbridge"));
                    kabcbridgecall = (kabcbridgecalltype)lib.resolveFunction("ktexteditorkabcbridge");
                    if (kabcbridgecall == 0) {
                        KMessageBox::sorry(parentWindow, i18n("The template needs information about you, which is stored in your address book.\nHowever, the required plugin could not be loaded.\n\nPlease install the KDEPIM/Kontact package for your system."));
                        return false;
                    }
                }
                bool ok;
                map[ placeholder ] = kabcbridgecall(placeholder, parentWindow, &ok);
                if (!ok) {
                    return false;
                }
            } else if (placeholder == QLatin1String("date")) {
                map[ placeholder ] =  QLocale().toString(date, QLocale::ShortFormat);
            } else if (placeholder == QLatin1String("time")) {
                map[ placeholder ] = QLocale().toString(time, QLocale::LongFormat);
            } else if (placeholder == QLatin1String("year")) {
                map[ placeholder ] = date.toString(QLatin1String("yyyy"));
            } else if (placeholder == QLatin1String("month")) {
                map[ placeholder ] = date.toString(QLatin1String("MM"));
            } else if (placeholder == QLatin1String("day")) {
                map[ placeholder ] = date.toString(QLatin1String("dd"));
            } else if (placeholder == QLatin1String("hostname")) {
                map[ placeholder ] = QHostInfo::localHostName();
            } else if (placeholder == QLatin1String("cursor")) {
                map[ placeholder ] = QLatin1Char('|');
            } else if (placeholder == QLatin1String("selection")) {
                //DO NOTHING, THE IMPLEMENTATION WILL HANDLE THIS
            } else {
                map[ placeholder ] = placeholder;
            }
        }
    }
    return true;
}

bool TemplateInterface::KTE_INTERNAL_setupIntialValues(const QString &templateString, QMap<QString, QString> *initialValues)
{
    QMap<QString, QString> enhancedInitValues(*initialValues);

    QRegExp rx(QLatin1String("[$%]\\{([^}\\r\\n]+)\\}"));
    rx.setMinimal(true);
    int pos = 0;
    int offset;
    QString initValue;
    while (pos >= 0) {
        bool initValue_specified = false;
        pos = rx.indexIn(templateString, pos);

        if (pos > -1) {
            offset = 0;
            while (pos - offset > 0 && templateString[ pos - offset - 1 ] == QLatin1Char('\\')) {
                ++offset;
            }
            if (offset % 2 == 1) {
                // match is escaped
                ++pos;
                continue;
            }
            QString placeholder = rx.cap(1);

            int pos_colon = placeholder.indexOf(QLatin1Char(':'));
            int pos_slash = placeholder.indexOf(QLatin1Char('/'));
            int pos_backtick = placeholder.indexOf(QLatin1Char('`'));
            bool check_slash = false;
            bool check_colon = false;
            bool check_backtick = false;
            if ((pos_colon == -1) && (pos_slash == -1)) {
                //do nothing
            } else if ((pos_colon == -1) && (pos_slash != -1)) {
                check_slash = true;
            } else if ((pos_colon != -1) && (pos_slash == -1)) {
                check_colon = true;
            } else {
                if (pos_colon < pos_slash) {
                    check_colon = true;
                } else {
                    check_slash = true;
                }
            }

            if ((!check_slash) && (!check_colon) && (pos_backtick >= 0)) {
                check_backtick = true;
            }

            if (check_slash) {
                //in most cases it should not matter, but better safe then sorry.
                const int end = placeholder.length();
                int slashcount = 0;
                int backslashcount = 0;
                for (int i = 0; i < end; i++) {
                    if (placeholder[i] == QLatin1Char('/')) {
                        if ((backslashcount % 2) == 0) {
                            slashcount++;
                        }
                        if (slashcount == 3) {
                            break;
                        }
                        backslashcount = 0;
                    } else if (placeholder[i] == QLatin1Char('\\')) {
                        backslashcount++;
                    } else {
                        backslashcount = 0;    //any character terminates a backslash sequence
                    }
                }
                if (slashcount != 3) {
                    const int tmpStrLength = templateString.length();
                    for (int i = pos + rx.matchedLength(); (slashcount < 3) && (i < tmpStrLength); i++, pos++) {
                        if (templateString[i] == QLatin1Char('/')) {
                            if ((backslashcount % 2) == 0) {
                                slashcount++;
                            }
                            backslashcount = 0;
                        } else if (placeholder[i] == QLatin1Char('\\')) {
                            backslashcount++;
                        } else {
                            backslashcount = 0;    //any character terminates a backslash sequence
                        }
                    }
                }
                //this is needed
                placeholder = placeholder.left(placeholder.indexOf(QLatin1Char('/')));
            } else if (check_colon) {
                initValue = placeholder.mid(pos_colon + 1);
                initValue_specified = true;
                int  backslashcount = 0;
                for (int i = initValue.length() - 1; (i >= 0) && (initValue[i] == QLatin1Char('\\')); i--) {
                    backslashcount++;
                }
                initValue = initValue.left(initValue.length() - ((backslashcount + 1) / 2));
                if ((backslashcount % 2) == 1) {
                    initValue += QLatin1Char('}');
                    const int tmpStrLength = templateString.length();
                    backslashcount = 0;
                    for (int i = pos + rx.matchedLength(); (i < tmpStrLength); i++, pos++) {
                        if (templateString[i] == QLatin1Char('}')) {
                            initValue = initValue.left(initValue.length() - ((backslashcount + 1) / 2));
                            if ((backslashcount % 2) == 0) {
                                break;
                            }
                            backslashcount = 0;
                        } else if (placeholder[i] == QLatin1Char('\\')) {
                            backslashcount++;
                        } else {
                            backslashcount = 0;    //any character terminates a backslash sequence
                        }
                        initValue += placeholder[i];
                    }
                }
                placeholder = placeholder.left(placeholder.indexOf(QLatin1Char(':')));
            } else if (check_backtick) {
                placeholder = placeholder.left(pos_backtick);
            }

            if (placeholder.contains(QLatin1Char('@'))) {
                placeholder = placeholder.left(placeholder.indexOf(QLatin1Char('@')));
            }
            if ((! enhancedInitValues.contains(placeholder)) || (enhancedInitValues[placeholder] == DUMMY_VALUE)) {
                if (initValue_specified) {
                    enhancedInitValues[placeholder] = initValue;
                } else {
                    enhancedInitValues[ placeholder ] = DUMMY_VALUE;
                }
            }
            pos += rx.matchedLength();
        }
    }

    for (QMap<QString, QString>::iterator it = enhancedInitValues.begin(); it != enhancedInitValues.end(); ++it) {
        if (it.value() == DUMMY_VALUE) {
            it.value() = QString();
        }
    }
    if (!expandMacros(enhancedInitValues, dynamic_cast<QWidget *>(this))) {
        return false;
    }
    *initialValues = enhancedInitValues;
    return true;
}

bool TemplateInterface::insertTemplateText(const Cursor &insertPosition, const QString &templateString, const QMap<QString, QString> &initialValues)
{
    QMap<QString, QString> enhancedInitValues(initialValues);
    if (!KTE_INTERNAL_setupIntialValues(templateString, &enhancedInitValues)) {
        return false;
    }
    return insertTemplateTextImplementation(insertPosition, templateString, enhancedInitValues);
}
