/* This file is part of qjson
  *
  * Copyright (C) 2010 Flavio Castelli <flavio@castelli.name>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License version 2.1, as published by the Free Software Foundation.
  * 
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with this library; see the file COPYING.LIB.  If not, write to
  * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301, USA.
  */

#include <QtCore/QStringBuilder>
#include <QtGui/QMessageBox>
#include <stdio.h>

#include "cmdlineparser.h"

using namespace QJson;

const QString CmdLineParser::m_helpMessage = QLatin1String(
        "Usage: cmdline_tester [options] file\n\n"
        "This program converts the json data read from 'file' to a QVariant object.\n"
        "--serialize Parses the QVariant object back to json.\n"
        "--indent    Sets the indentation level used by the 'serialize' option.\n"
        "            Allowed values:\n"
        "            - none [default]\n"
        "            - minimum\n"
        "            - medium\n"
        "            - full\n"
        "--help      Displays this help.\n"
        );


CmdLineParser::CmdLineParser(const QStringList &arguments)
    : m_pos(0),
      m_indentationMode(IndentNone),
      m_serialize(false)
{
    for (int i = 1; i < arguments.count(); ++i) {
        const QString &arg = arguments.at(i);
        m_arguments.append(arg);
    }
}

CmdLineParser::Result CmdLineParser::parse()
{
    bool showHelp = false;

    while (m_error.isEmpty() && hasMoreArgs()) {
        const QString &arg = nextArg().toLower();
        if (arg == QLatin1String("--indent"))
            handleSetIndentationMode();
        else if (arg == QLatin1String("--help"))
            showHelp = true;
        else if (arg == QLatin1String("--serialize"))
            m_serialize = true;
        else if (!arg.startsWith(QLatin1String("--")))
            m_file = arg;
        else
            m_error = QString(QLatin1String("Unknown option: %1")).arg(arg);
    }

    if (m_file.isEmpty()) {
      m_error = QLatin1String("You have to specify the file containing the json data.");
    }

    if (!m_error.isEmpty()) {
        showMessage(m_error + QLatin1String("\n\n\n") + m_helpMessage, true);
        return Error;
    } else if (showHelp) {
        showMessage(m_helpMessage, false);
        return Help;
    }
    return Ok;
}

bool CmdLineParser::hasMoreArgs() const
{
    return m_pos < m_arguments.count();
}

const QString &CmdLineParser::nextArg()
{
    Q_ASSERT(hasMoreArgs());
    return m_arguments.at(m_pos++);
}

void CmdLineParser::handleSetIndentationMode()
{
    if (hasMoreArgs()) {
        const QString &indentationMode = nextArg();
        if (indentationMode.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
          m_indentationMode = IndentNone;
        else if (indentationMode.compare(QLatin1String("minimum"), Qt::CaseInsensitive) == 0)
          m_indentationMode = IndentMinimum;
        else if (indentationMode.compare(QLatin1String("medium"), Qt::CaseInsensitive) == 0)
          m_indentationMode = IndentMedium;
        else if (indentationMode.compare(QLatin1String("full"), Qt::CaseInsensitive) == 0)
          m_indentationMode = IndentFull;
        else
          m_error = QString(QLatin1String("Unknown indentation mode '%1'.")).
                                          arg(indentationMode);
    } else {
        m_error = QLatin1String("Missing indentation level.");
    }
}

void CmdLineParser::showMessage(const QString &msg, bool error)
{
#ifdef Q_OS_WIN
    QString message = QLatin1String("<pre>") % msg % QLatin1String("</pre>");
    if (error)
        QMessageBox::critical(0, QLatin1String("Error"), message);
    else
        QMessageBox::information(0, QLatin1String("Notice"), message);
#else
    fprintf(error ? stderr : stdout, "%s\n", qPrintable(msg));
#endif
}

void CmdLineParser::setIndentationMode(const IndentMode &mode)
{
    m_indentationMode = mode;
}

IndentMode CmdLineParser::indentationMode() const
{
    return m_indentationMode;
}

QString CmdLineParser::file() const
{
    return m_file;
}

bool CmdLineParser::serialize()
{
    return m_serialize;
}

