/*

Copyright 2017, Rodrigo Rivas Costa <rodrigorivascosta@gmail.com>

This file is part of inputmap.

inputmap is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

inputmap is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with inputmap.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "devinput-parser.h"
#include "devinput.h"

int ValueRef::get_value()
{
    auto dev = m_device.lock();
    if (!dev)
        return 0;
    return dev->get_value(m_value_id);
}

int ValueTuple::get_value()
{
    int v1 = m_r1->get_value();
    int v2 = m_r2->get_value();
    if (v1 > 0)
        return 32767;
    else if (v2 > 0)
        return -32767;
    else
        return 0;
}

int ValueCond::get_value()
{
    int c = m_cond->get_value();
    return (c ? m_true : m_false)->get_value();
}

int ValueOper::get_value()
{
    switch (m_oper)
    {
    case InputToken_PLUS:
        return m_left->get_value() + m_right->get_value();
    case InputToken_MINUS:
        return m_left->get_value() - m_right->get_value();
    case InputToken_LT:
        return m_left->get_value() < m_right->get_value()? 1 : 0;
    case InputToken_GT:
        return m_left->get_value() > m_right->get_value()? 1 : 0;
    case InputToken_AND:
        return m_left->get_value() && m_right->get_value()? 1 : 0;
    case InputToken_OR:
        return m_left->get_value() || m_right->get_value()? 1 : 0;
    default:
        return 0;
    }
}

////////////////////

enum class CharCategory
{
    Space,
    Letter,
    Number,
    Other,
};

CharCategory char_category(char c)
{
    if (c >= '0' && c <= '9')
        return CharCategory::Number;
    else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
        return CharCategory::Letter;
    else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        return CharCategory::Space;
    else
        return CharCategory::Other;
}

CharCategory next_token(const std::string &txt, size_t &pos)
{
    char c = txt[pos++];

    CharCategory cc = char_category(c);
    if (cc == CharCategory::Other)
        return cc;

    for (; pos < txt.size(); ++pos)
    {
        char c2 = txt[pos];
        CharCategory cc2 = char_category(c2);
        if (cc2 != cc)
            break;
    }
    return cc;
}

ValueRef *create_value_ref(const std::string &sdev, const std::string &saxis, IInputByName &finder)
{
    auto dev = finder.find_input(sdev);
    if (!dev)
        throw std::runtime_error("unknown device in ref: " + sdev + "." + saxis);
    auto value_id = dev->parse_value(saxis);
    return new ValueRef(dev, value_id);
}

std::unique_ptr<ValueExpr> parse_ref(const std::string &desc, IInputByName &finder)
{
    DevInputArgs args{ finder };
    void *parser = DevInputParseAlloc(malloc);
    size_t pos = 0;
    while (pos < desc.size() && !args.error)
    {
        size_t pos0 = pos;
        CharCategory cc = next_token(desc, pos);
        //printf("%d: %d %d <%s>\n", (int)cc, pos0, pos, std::string(desc, pos0, pos-pos0).c_str());
        switch (cc)
        {
        case CharCategory::Space:
            break;
        case CharCategory::Letter:
            {
                std::string s(desc, pos0, pos - pos0);
                if (s == "and")
                    DevInputParse(parser, InputToken_AND, 0, &args);
                else if (s == "or")
                    DevInputParse(parser, InputToken_OR, 0, &args);
                else
                    DevInputParse(parser, InputToken_NAME, new std::string(std::move(s)), &args);
            }
            break;
        case CharCategory::Number:
            {
                std::string *s = new std::string(desc, pos0, pos - pos0);
                DevInputParse(parser, InputToken_NUMBER, s, &args);
            }
            break;
        case CharCategory::Other:
            switch (desc[pos0])
            {
            case '(':
                DevInputParse(parser, InputToken_LPAREN, 0, &args);
                break;
            case ')':
                DevInputParse(parser, InputToken_RPAREN, 0, &args);
                break;
            case '.':
                DevInputParse(parser, InputToken_PERIOD, 0, &args);
                break;
            case ',':
                DevInputParse(parser, InputToken_COMMA, 0, &args);
                break;
            case ':':
                DevInputParse(parser, InputToken_COLON, 0, &args);
                break;
            case '?':
                DevInputParse(parser, InputToken_QUESTION, 0, &args);
                break;
            case '+':
                DevInputParse(parser, InputToken_PLUS, 0, &args);
                break;
            case '-':
                DevInputParse(parser, InputToken_MINUS, 0, &args);
                break;
            case '>':
                DevInputParse(parser, InputToken_GT, 0, &args);
                break;
            case '<':
                DevInputParse(parser, InputToken_LT, 0, &args);
                break;
            default:
                printf("*** unknown character %c\n", desc[pos0]);
                break;
            }
            break;
        }
    }
    DevInputParse(parser, 0, 0, &args);
    DevInputParseFree(parser, free);

    if (args.error || !args.input)
        throw std::runtime_error("invalid input expression: " + desc);

    return std::unique_ptr<ValueExpr>(args.input);
}


