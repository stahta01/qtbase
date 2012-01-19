/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: http://www.qt-project.org/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "main.h"

#include <QFile>
#include <QHash>
#include <QString>
#include <QStringList>

#include <QTest>


class tst_QHash : public QObject
{
    Q_OBJECT

private slots:
    void qhash_qt4_data() { data(); }
    void qhash_qt4();
    void qhash_faster_data() { data(); }
    void qhash_faster();
    void javaString_data() { data(); }
    void javaString();

private:
    void data();
};

const int N = 1000000;
extern double s;

///////////////////// QHash /////////////////////

void tst_QHash::data()
{
    QFile smallPathsData("paths_small_data.txt");
    smallPathsData.open(QIODevice::ReadOnly);

    QTest::addColumn<QStringList>("items");
    QTest::newRow("paths-small")
            << QString::fromLatin1(smallPathsData.readAll()).split(QLatin1Char('\n'));
}

void tst_QHash::qhash_qt4()
{
    QFETCH(QStringList, items);
    QStringList realitems = items; // for copy/paste ease between benchmarks
    QHash<QString, int> hash;

    QBENCHMARK {
        for (int i = 0, n = realitems.size(); i != n; ++i) {
            hash[realitems.at(i)] = i;
        }
    }
}

void tst_QHash::qhash_faster()
{
    QFETCH(QStringList, items);
    QHash<String, int> hash;

    QList<String> realitems;
    foreach (const QString &s, items)
        realitems.append(s);

    QBENCHMARK {
        for (int i = 0, n = realitems.size(); i != n; ++i) {
            hash[realitems.at(i)] = i;
        }
    }
}

void tst_QHash::javaString()
{
    QFETCH(QStringList, items);
    QHash<JavaString, int> hash;

    QList<JavaString> realitems;
    foreach (const QString &s, items)
        realitems.append(s);

    QBENCHMARK {
        for (int i = 0, n = realitems.size(); i != n; ++i) {
            hash[realitems.at(i)] = i;
        }
    }
}


QTEST_MAIN(tst_QHash)

#include "main.moc"
