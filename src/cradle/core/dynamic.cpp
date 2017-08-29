#include <cradle/core/dynamic.hpp>


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

dynamic::dynamic(std::initializer_list<dynamic> list)
{
    // If this is a list of lists, all of which are length two and have strings as their
    // first elements, treat it as a map.
    if (std::all_of(
            list.begin(), list.end(),
            [ ](dynamic const& v)
            {
                return
                    v.type() == value_type::LIST &&
                    cast<dynamic_array>(v).size() == 2 &&
                    cast<dynamic_array>(v)[0].type() == value_type::STRING;
            }))
    {
        dynamic_map map;
        for (auto v : list)
        {
            auto const& array = cast<dynamic_array>(v);
            map[array[0]] = array[1];
        }
        *this = std::move(map);
    }
    else
    {
        *this = dynamic_array(list);
    }
}

void dynamic::get(bool const** v) const
{
    check_type(value_type::BOOLEAN, type_);
    *v = boost::any_cast<bool>(&value_);
}
void dynamic::get(integer const** v) const
{
    check_type(value_type::INTEGER, type_);
    *v = boost::any_cast<integer>(&value_);
}
void dynamic::get(double const** v) const
{
    check_type(value_type::FLOAT, type_);
    *v = boost::any_cast<double>(&value_);
}
void dynamic::get(string const** v) const
{
    check_type(value_type::STRING, type_);
    *v = boost::any_cast<string>(&value_);
}
void dynamic::get(blob const** v) const
{
    check_type(value_type::BLOB, type_);
    *v = boost::any_cast<blob>(&value_);
}
void dynamic::get(boost::posix_time::ptime const** v) const
{
    check_type(value_type::DATETIME, type_);
    *v = boost::any_cast<boost::posix_time::ptime>(&value_);
}
void dynamic::get(dynamic_array const** v) const
{
    // Certain ways of encoding values (e.g., JSON) have the same representation for empty
    // arrays and empty maps, so if we encounter an empty map here, we should treat it as
    // an empty list.
    if (type_ == value_type::MAP && cast<dynamic_map>(*this).empty())
    {
        static const dynamic_array empty_list;
        *v = &empty_list;
        return;
    }
    check_type(value_type::LIST, type_);
    *v = boost::any_cast<dynamic_array>(&value_);
}
void dynamic::get(dynamic_map const** v) const
{
    // Same logic as in the list case.
    if (type_ == value_type::LIST && cast<dynamic_array>(*this).empty())
    {
        static const dynamic_map empty_map;
        *v = &empty_map;
        return;
    }
    check_type(value_type::MAP, type_);
    *v = boost::any_cast<dynamic_map>(&value_);
}

void dynamic::set(nil_t _)
{
    type_ = value_type::NIL;
}
void dynamic::set(bool v)
{
    type_ = value_type::BOOLEAN;
    value_ = v;
}
void dynamic::set(integer v)
{
    type_ = value_type::INTEGER;
    value_ = v;
}
void dynamic::set(double v)
{
    type_ = value_type::FLOAT;
    value_ = v;
}
void dynamic::set(string const& v)
{
    type_ = value_type::STRING;
    value_ = v;
}
void dynamic::set(string&& v)
{
    type_ = value_type::STRING;
    value_ = std::move(v);
}
void dynamic::set(blob const& v)
{
    type_ = value_type::BLOB;
    value_ = v;
}
void dynamic::set(blob&& v)
{
    type_ = value_type::BLOB;
    value_ = std::move(v);
}
void dynamic::set(boost::posix_time::ptime const& v)
{
    type_ = value_type::DATETIME;
    value_ = v;
}
void dynamic::set(boost::posix_time::ptime&& v)
{
    type_ = value_type::DATETIME;
    value_ = std::move(v);
}
void dynamic::set(dynamic_array const& v)
{
    type_ = value_type::LIST;
    value_ = v;
}
void dynamic::set(dynamic_array&& v)
{
    type_ = value_type::LIST;
    value_ = std::move(v);
}
void dynamic::set(dynamic_map const& v)
{
    type_ = value_type::MAP;
    value_ = v;
}
void dynamic::set(dynamic_map&& v)
{
    type_ = value_type::MAP;
    value_ = std::move(v);
}

void swap(dynamic& a, dynamic& b)
{
    using std::swap;
    swap(a.type_, b.type_);
    swap(a.value_, b.value_);
}

std::ostream&
operator<<(std::ostream& os, dynamic const& v)
{
    os << value_to_json(v);
    return os;
}

size_t deep_sizeof(dynamic const& v)
{
    return sizeof(dynamic) + apply_to_dynamic(CRADLE_LAMBDIFY(deep_sizeof), v);
}

size_t hash_value(dynamic const& x)
{
    return apply_to_dynamic(CRADLE_LAMBDIFY(invoke_hash), x);
}

// COMPARISON OPERATORS

bool operator==(dynamic const& a, dynamic const& b)
{
    if (a.type() != b.type())
        return false;
    return
        apply_to_dynamic_pair(
            [ ](auto const& x, auto const& y)
            {
                return x == y;
            },
            a,
            b);
}
bool operator!=(dynamic const& a, dynamic const& b)
{ return !(a == b); }

bool operator<(dynamic const& a, dynamic const& b)
{
    if (a.type() != b.type())
        return a.type() < b.type();
    return
        apply_to_dynamic_pair(
            [ ](auto const& x, auto const& y)
            {
                return x < y;
            },
            a,
            b);
}
bool operator<=(dynamic const& a, dynamic const& b)
{ return !(b < a); }
bool operator>(dynamic const& a, dynamic const& b)
{ return b < a; }
bool operator>=(dynamic const& a, dynamic const& b)
{ return !(a < b); }

dynamic const&
get_field(dynamic_map const& r, string const& field)
{
    dynamic const* v;
    if (!get_field(&v, r, field))
    {
        CRADLE_THROW(missing_field() << field_name_info(field));
    }
    return *v;
}

bool
get_field(dynamic const** v, dynamic_map const& r, string const& field)
{
    auto i = r.find(dynamic(field));
    if (i == r.end())
        return false;
    *v = &i->second;
    return true;
}

dynamic const&
get_union_value_type(dynamic_map const& map)
{
    if (map.size() != 1)
    {
        CRADLE_THROW(multifield_union());
    }
    return map.begin()->first;
}

void
add_dynamic_path_element(boost::exception& e, dynamic const& path_element)
{
    std::list<dynamic>* info = get_error_info<dynamic_value_path_info>(e);
    if (info)
    {
        info->push_front(path_element);
    }
    else
    {
        e << dynamic_value_path_info(std::list<dynamic>({ path_element }));
    }
}

}
