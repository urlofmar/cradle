#include <cradle/core/value.hpp>


#include <algorithm>

#include <cradle/common.hpp>
#include <cradle/encodings/json.hpp>

namespace cradle {

std::ostream& operator<<(std::ostream& s, value_type t)
{
    switch (t)
    {
     case value_type::NIL:
        s << "nil";
        break;
     case value_type::BOOLEAN:
        s << "boolean";
        break;
     case value_type::INTEGER:
        s << "integer";
        break;
     case value_type::FLOAT:
        s << "float";
        break;
     case value_type::STRING:
        s << "string";
        break;
     case value_type::BLOB:
        s << "blob";
        break;
     case value_type::DATETIME:
        s << "datetime";
        break;
     case value_type::LIST:
        s << "list";
        break;
     case value_type::MAP:
        s << "map";
        break;
     default:
        CRADLE_THROW(
            invalid_enum_value() <<
                enum_id_info("value_type") <<
                enum_value_info(int(t)));
    }
    return s;
}

void check_type(value_type expected, value_type actual)
{
    if (expected != actual)
    {
        CRADLE_THROW(
            type_mismatch() <<
                expected_value_type_info(expected) <<
                actual_value_type_info(actual));
    }
}

value::value(std::initializer_list<value> list)
{
    // If this is a list of lists, all of which are length two and have strings as their
    // first elements, treat it as a map.
    if (std::all_of(
            list.begin(), list.end(),
            [ ](value const& v)
            {
                return
                    v.type() == value_type::LIST &&
                    cast<value_list>(v).size() == 2 &&
                    cast<value_list>(v)[0].type() == value_type::STRING;
            }))
    {
        value_map map;
        for (auto v : list)
        {
            auto const& array = cast<value_list>(v);
            map[array[0]] = array[1];
        }
        *this = std::move(map);
    }
    else
    {
        *this = value_list(list);
    }
}

void value::get(bool const** v) const
{
    check_type(value_type::BOOLEAN, type_);
    *v = boost::any_cast<bool>(&value_);
}
void value::get(integer const** v) const
{
    check_type(value_type::INTEGER, type_);
    *v = boost::any_cast<integer>(&value_);
}
void value::get(double const** v) const
{
    check_type(value_type::FLOAT, type_);
    *v = boost::any_cast<double>(&value_);
}
void value::get(string const** v) const
{
    check_type(value_type::STRING, type_);
    *v = boost::any_cast<string>(&value_);
}
void value::get(blob const** v) const
{
    check_type(value_type::BLOB, type_);
    *v = boost::any_cast<blob>(&value_);
}
void value::get(boost::posix_time::ptime const** v) const
{
    check_type(value_type::DATETIME, type_);
    *v = boost::any_cast<boost::posix_time::ptime>(&value_);
}
void value::get(value_list const** v) const
{
    // Certain ways of encoding values (e.g., JSON) have the same representation for empty
    // arrays and empty maps, so if we encounter an empty map here, we should treat it as
    // an empty list.
    if (type_ == value_type::MAP && cast<value_map>(*this).empty())
    {
        static const value_list empty_list;
        *v = &empty_list;
        return;
    }
    check_type(value_type::LIST, type_);
    *v = boost::any_cast<value_list>(&value_);
}
void value::get(value_map const** v) const
{
    // Same logic as in the list case.
    if (type_ == value_type::LIST && cast<value_list>(*this).empty())
    {
        static const value_map empty_map;
        *v = &empty_map;
        return;
    }
    check_type(value_type::MAP, type_);
    *v = boost::any_cast<value_map>(&value_);
}

void value::set(nil_t _)
{
    type_ = value_type::NIL;
}
void value::set(bool v)
{
    type_ = value_type::BOOLEAN;
    value_ = v;
}
void value::set(integer v)
{
    type_ = value_type::INTEGER;
    value_ = v;
}
void value::set(double v)
{
    type_ = value_type::FLOAT;
    value_ = v;
}
void value::set(string const& v)
{
    type_ = value_type::STRING;
    value_ = v;
}
void value::set(string&& v)
{
    type_ = value_type::STRING;
    value_ = std::move(v);
}
void value::set(blob const& v)
{
    type_ = value_type::BLOB;
    value_ = v;
}
void value::set(blob&& v)
{
    type_ = value_type::BLOB;
    value_ = std::move(v);
}
void value::set(boost::posix_time::ptime const& v)
{
    type_ = value_type::DATETIME;
    value_ = v;
}
void value::set(boost::posix_time::ptime&& v)
{
    type_ = value_type::DATETIME;
    value_ = std::move(v);
}
void value::set(value_list const& v)
{
    type_ = value_type::LIST;
    value_ = v;
}
void value::set(value_list&& v)
{
    type_ = value_type::LIST;
    value_ = std::move(v);
}
void value::set(value_map const& v)
{
    type_ = value_type::MAP;
    value_ = v;
}
void value::set(value_map&& v)
{
    type_ = value_type::MAP;
    value_ = std::move(v);
}

void swap(value& a, value& b)
{
    using std::swap;
    swap(a.type_, b.type_);
    swap(a.value_, b.value_);
}

std::ostream&
operator<<(std::ostream& os, value const& v)
{
    os << value_to_json(v);
    return os;
}

size_t deep_sizeof(value const& v)
{
    return sizeof(value) + apply_to_value(CRADLE_LAMBDIFY(deep_sizeof), v);
}

size_t hash_value(value const& x)
{
    return apply_to_value(CRADLE_LAMBDIFY(invoke_hash), x);
}

// COMPARISON OPERATORS

bool operator==(value const& a, value const& b)
{
    if (a.type() != b.type())
        return false;
    return
        apply_to_value_pair(
            [ ](auto const& x, auto const& y)
            {
                return x == y;
            },
            a,
            b);
}
bool operator!=(value const& a, value const& b)
{ return !(a == b); }

bool operator<(value const& a, value const& b)
{
    if (a.type() != b.type())
        return a.type() < b.type();
    return
        apply_to_value_pair(
            [ ](auto const& x, auto const& y)
            {
                return x < y;
            },
            a,
            b);
}
bool operator<=(value const& a, value const& b)
{ return !(b < a); }
bool operator>(value const& a, value const& b)
{ return b < a; }
bool operator>=(value const& a, value const& b)
{ return !(a < b); }

value const& get_field(value_map const& r, string const& field)
{
    value const* v;
    if (!get_field(&v, r, field))
    {
        CRADLE_THROW(missing_field() << field_name_info(field));
    }
    return *v;
}

bool get_field(value const** v, value_map const& r, string const& field)
{
    auto i = r.find(value(field));
    if (i == r.end())
        return false;
    *v = &i->second;
    return true;
}

value const& get_union_value_type(value_map const& map)
{
    if (map.size() != 1)
    {
        CRADLE_THROW(multifield_union());
    }
    return map.begin()->first;
}

void
add_dynamic_path_element(boost::exception& e, value const& path_element)
{
    std::list<value>* info = get_error_info<dynamic_value_path_info>(e);
    if (info)
    {
        info->push_front(path_element);
    }
    else
    {
        e << dynamic_value_path_info(std::list<value>({ path_element }));
    }
}

}
