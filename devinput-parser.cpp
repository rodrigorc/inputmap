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
        {
            int a = m_left->get_value();
            return a ? m_right->get_value() : 0;
        };
    case InputToken_OR:
        {
            int a = m_left->get_value();
            return a ? a : m_right->get_value();
        }
    case InputToken_MULT:
        return m_left->get_value() * m_right->get_value();
    case InputToken_DIV:
        {
            int r = m_right->get_value();
            if (r != 0)
                return m_left->get_value() / r;
            else
                return 0;
        }
    default:
        return 0;
    }
}

//////////////////////////
// Functions

class ValueFunc2 : public ValueExpr
{
public:
    ValueFunc2(int (*f)(int,int), std::unique_ptr<ValueExpr> &&e1, std::unique_ptr<ValueExpr> &&e2)
        :m_fun(f), m_e1(std::move(e1)), m_e2(std::move(e2))
    {
    }
    int get_value() override
    {
        return m_fun(m_e1->get_value(), m_e2->get_value());
    }
private:
    int (*m_fun)(int,int);
    std::unique_ptr<ValueExpr> m_e1, m_e2;
};

class ValueFunc3 : public ValueExpr
{
public:
    ValueFunc3(int (*f)(int,int,int), std::unique_ptr<ValueExpr> &&e1, std::unique_ptr<ValueExpr> &&e2, std::unique_ptr<ValueExpr> &&e3)
        :m_fun(f), m_e1(std::move(e1)), m_e2(std::move(e2)), m_e3(std::move(e3))
    {
    }
    int get_value() override
    {
        return m_fun(m_e1->get_value(), m_e2->get_value(), m_e3->get_value());
    }
private:
    int (*m_fun)(int,int,int);
    std::unique_ptr<ValueExpr> m_e1, m_e2, m_e3;
};

ValueExpr *create_func_ex(int(*f)(int,int), std::vector<std::unique_ptr<ValueExpr>> &&exprs)
{
    if (exprs.size() != 2)
        throw std::runtime_error("wrong number of arguments in function");
    return new ValueFunc2(f, std::move(exprs[0]), std::move(exprs[1]));
}
ValueExpr *create_func_ex(int(*f)(int,int,int), std::vector<std::unique_ptr<ValueExpr>> &&exprs)
{
    if (exprs.size() != 3)
        throw std::runtime_error("wrong number of arguments in function");
    return new ValueFunc3(f, std::move(exprs[0]), std::move(exprs[1]), std::move(exprs[2]));
}

int func_between(int a, int b, int c)
{
    if (b < c)
        return b <= a && a < c;
    else
        return c <= a && a < b;
}

class ValueMouse : public ValueExpr
{
public:
    ValueMouse(std::unique_ptr<ValueExpr> touch, std::unique_ptr<ValueExpr> x)
        :m_touch(std::move(touch)), m_x(std::move(x)), m_touching(false)
    {
    }
    int get_value() override
    {
        int touch = m_touch->get_value();
        if (!touch)
        {
            m_touching = false;
            return 0;
        }
        int x = m_x->get_value();
        int old = m_old;
        m_old = x;
        if (!m_touching)
        {
            m_touching = true;
            return 0;
        }
        return x - old;
    }
private:
    std::unique_ptr<ValueExpr> m_touch, m_x, m_fuzz;
    bool m_touching;
    int m_old;
};

class ValueDefuzz : public ValueExpr
{
public:
    ValueDefuzz(std::unique_ptr<ValueExpr> x, std::unique_ptr<ValueExpr> fuzz)
        :m_x(std::move(x)), m_fuzz(std::move(fuzz)), m_old(0)
    {
    }
    int get_value() override
    {
        int x = m_x->get_value();
        int old = m_old;
        int fuzz = m_fuzz->get_value();
	if (fuzz)
        {
            if (old - fuzz / 2 < x && x < old + fuzz / 2)
                x = old;

            if (old - fuzz < x && x < old + fuzz)
                x = (old * 3 + x) / 4;

            if (old - fuzz * 2 < x && x < old + fuzz * 2)
                x = (old + x) / 2;
        }
        m_old = x;
        return x;
    }
private:
    std::unique_ptr<ValueExpr> m_x, m_fuzz;
    bool m_touching;
    int m_old;
};

class ValueTurbo : public ValueExpr
{
public:
    ValueTurbo(std::unique_ptr<ValueExpr> x)
        :m_x(std::move(x)), m_clicked(false)
    {
    }
    int get_value() override
    {
        int x = m_x->get_value();
        if (x)
            m_clicked = !m_clicked;
        else
            m_clicked = false;
        return m_clicked? 1 : 0;
    }
private:
    std::unique_ptr<ValueExpr> m_x;
    bool m_clicked;
};

class ValueToggle : public ValueExpr
{
public:
    ValueToggle(std::unique_ptr<ValueExpr> x)
        :m_x(std::move(x)), m_prev(false)
    {
    }
    int get_value() override
    {
        bool x = m_x->get_value() != 0;
        if (!m_prev && x) //edge on
            m_clicked = !m_clicked;
        m_prev = x;
        return m_clicked? 1 : 0;
    }
private:
    std::unique_ptr<ValueExpr> m_x;
    bool m_prev, m_clicked;
};

ValueExpr* create_func(const std::string &name, std::vector<std::unique_ptr<ValueExpr>> &&exprs)
{
    try
    {
        if (name == "between")
        {
            return create_func_ex(func_between, std::move(exprs));
        }
        else if (name == "mouse")
        {
            if (exprs.size() != 2)
                throw std::runtime_error("wrong number of arguments in function");
            return new ValueMouse(std::move(exprs[0]), std::move(exprs[1]));
        }
        else if (name == "defuzz")
        {
            if (exprs.size() != 2)
                throw std::runtime_error("wrong number of arguments in function");
            return new ValueDefuzz(std::move(exprs[0]), std::move(exprs[1]));
        }
        else if (name == "turbo")
        {
            if (exprs.size() != 1)
                throw std::runtime_error("wrong number of arguments in function");
            return new ValueTurbo(std::move(exprs[0]));
        }
        else if (name == "toggle")
        {
            if (exprs.size() != 1)
                throw std::runtime_error("wrong number of arguments in function");
            return new ValueToggle(std::move(exprs[0]));
        }
        else
            throw std::runtime_error("unknown function");
    }
    catch (std::runtime_error e)
    {
        throw std::runtime_error(std::string(e.what()) + ": " + name);
    }
    return 0;
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
        if (cc == CharCategory::Letter && cc2 == CharCategory::Number)
            continue;
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
            case '*':
                DevInputParse(parser, InputToken_MULT, 0, &args);
                break;
            case '/':
                DevInputParse(parser, InputToken_DIV, 0, &args);
                break;
            case '$':
                DevInputParse(parser, InputToken_DOLLAR, 0, &args);
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


