#ifndef DEVINPUT_PARSER_H_INCLUDED
#define DEVINPUT_PARSER_H_INCLUDED

#include <string>
#include <memory>
#include "inputdev.h"

struct ValueExpr;

struct DevInputArgs
{
    IInputByName &finder;
    ValueExpr *input;
    bool error;
};

void *DevInputParseAlloc(void *(*mallocProc)(size_t));
void DevInputParseFree(void *p, void (*freeProc)(void*));
void DevInputParse(void *yyp, int yymajor, const std::string *yyminor, DevInputArgs *args);

struct ValueExpr
{
    virtual ~ValueExpr() =default;
    virtual int get_value() =0;
};

class ValueConst : public ValueExpr
{
public:
    ValueConst(int val)
        :value(val)
    {
    }
    int get_value() override { return value; }
private:
    int value;
};

class ValueRef : public ValueExpr
{
public:
    ValueRef(std::shared_ptr<InputDevice> dev, ValueId id)
        :m_device(dev), m_value_id(id)
    {
    }
    int get_value() override;
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

ValueRef *create_value_ref(const std::string &sdev, const std::string &saxis, IInputByName &finder);

std::unique_ptr<ValueExpr> parse_ref(const std::string &desc, IInputByName &finder);

#endif /* DEVINPUT_PARSER_H_INCLUDED */

