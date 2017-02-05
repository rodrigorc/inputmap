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

#ifndef DEVINPUT_PARSER_H_INCLUDED
#define DEVINPUT_PARSER_H_INCLUDED

#include <string>
#include <memory>
#include "inputdev.h"

struct ValueExpr
{
    virtual ~ValueExpr() {}
    virtual int get_value() =0;
};


class Variable
{
public:
    Variable(std::unique_ptr<ValueExpr> e)
        :m_expr(std::move(e)), m_value(0)
    {}
    void evaluate()
    {
        m_value = m_expr->get_value();
    }
    int get_value() const
    {
        return m_value;
    }
private:
    std::unique_ptr<ValueExpr> m_expr;
    int m_value;
};

struct IInputByName
{
    virtual ~IInputByName() {}
    virtual std::shared_ptr<InputDevice> find_input(const std::string &name) =0;
    virtual Variable *find_variable(const std::string &name) =0;
};

struct DevInputArgs
{
    IInputByName &finder;
    ValueExpr *input;
    bool error;
};

void *DevInputParseAlloc(void *(*mallocProc)(size_t));
void DevInputParseFree(void *p, void (*freeProc)(void*));
void DevInputParse(void *yyp, int yymajor, const std::string *yyminor, DevInputArgs *args);

class ValueConst : public ValueExpr
{
public:
    ValueConst(int val)
        :m_value(val)
    {
    }
    int get_value() override { return m_value; }
private:
    int m_value;
};

class ValueRef : public ValueExpr
{
public:
    ValueRef(std::shared_ptr<InputDevice> dev, ValueId id)
        :m_device(dev), m_value_id(id)
    {
    }
    int get_value() override;
    std::shared_ptr<InputDevice> get_device()
    {
        return m_device.lock();
    }
    const ValueId &get_value_id() const
    {
        return m_value_id;
    }
private:
    std::weak_ptr<InputDevice> m_device;
    ValueId m_value_id;
};

class ValueTuple : public ValueExpr
{
public:
    ValueTuple(ValueExpr *r1, ValueExpr *r2)
        :m_r1(r1), m_r2(r2)
    {
    }
    int get_value() override;
private:
    std::unique_ptr<ValueExpr> m_r1, m_r2;
};

class ValueCond : public ValueExpr
{
public:
    ValueCond(ValueExpr *c, ValueExpr *t, ValueExpr *f)
        :m_cond(c), m_true(t), m_false(f)
    {
    }
    int get_value() override;
private:
    std::unique_ptr<ValueExpr> m_cond, m_true, m_false;
};

class ValueOper : public ValueExpr
{
public:
    ValueOper(int oper, ValueExpr *l, ValueExpr *r)
        :m_oper(oper), m_left(l), m_right(r)
    {
    }
    int get_value() override;
private:
    int m_oper;
    std::unique_ptr<ValueExpr> m_left, m_right;
};

class ValueVariable : public ValueExpr
{
public:
    ValueVariable(const Variable *var)
        :m_var(var)
    {
    }
    int get_value() override { return m_var->get_value(); }
private:
    const Variable *m_var;
};

ValueRef *create_value_ref(const std::string &sdev, const std::string &saxis, IInputByName &finder);
ValueExpr* create_func(const std::string &name, std::vector<std::unique_ptr<ValueExpr>> &&exprs);

std::unique_ptr<ValueExpr> parse_ref(const std::string &desc, IInputByName &finder);

#endif /* DEVINPUT_PARSER_H_INCLUDED */

