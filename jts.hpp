#pragma once

#include <cstdint>
#include <cstring>
#include <ostream>

#include <iterator>

using namespace std;

namespace jts
{
    struct jts_element
    {
        friend class jts;

    private:
        uint8_t value;

    public:
        size_t utf8char(uint32_t *result) const
        {
            const uint8_t *p = &value;
            if ((*p & 0x80) == 0)
            {
                *result = *p & 0x7f;
                return 1;
            }
            else if ((*p & 0xe0) == 0xc0)
            {
                *result = (p[0] & 0x1f) << 6 | (p[1] & 0x3f);
                return 2;
            }
            else if ((*p & 0xf0) == 0xe0)
            {
                *result = (p[0] & 0x0f) << 12 | (p[1] & 0x3f) << 6 | (p[2] & 0x3f);
                return 3;
            }
            *result = (p[0] & 0x07) << 18 | (p[1] & 0x3f) << 12 | (p[2] & 0x3f) << 6 | (p[3] & 0x3f);
            return 4;
        }

    public:
        jts_element() = default;
        jts_element(uint8_t v) : value(v) {}

    public:
        uint32_t to_unicode() const
        {
            uint32_t result = 0;
            utf8char(&result);
            return result;
        }

    public:
        jts_element *next() const
        {
            uint32_t result = 0;
            auto size = utf8char(&result);
            uint8_t *ptr = (uint8_t *)&value + size;
            return (jts_element *)(ptr);
        }
        jts_element *prev() const
        {
            uint8_t *ptr = (uint8_t *)&value;
            while ((*(--ptr) & 0xc0) == 0x80)
                ;
            return (jts_element *)(ptr);
        }
        jts_element *traverse(int delta) const
        {
            jts_element *ptr = (jts_element *)this;
            if (delta > 0)
            {
                for (int i = 0; i < delta; i++)
                {
                    ptr = ptr->next();
                }
            }
            else
            {
                for (int i = 0; i > delta; i--)
                {
                    ptr = ptr->prev();
                }
            }
            return ptr;
        }
        ptrdiff_t distance(jts_element *other) const
        {
            ptrdiff_t result = 0;
            auto ptr = this;
            while (ptr != other)
            {
                if (other > ptr)
                {
                    ptr = ptr->next();
                    result++;
                }
                else
                {
                    ptr = ptr->prev();
                    result--;
                }
            }
            return result;
        }

        size_t size() const
        {
            uint32_t tmp;

            auto result = utf8char(&tmp);
            return result;
        }

    public:
        operator int() const { return to_unicode(); }
        operator char() const { return (char)to_unicode(); }
    };

    class jts
    {
    public:
        struct iterator
        {
            using difference_type = std::ptrdiff_t;
            using element_type = jts_element; // element_type is a reserved name that must be used in the definition
            using pointer = element_type *;
            using reference = element_type &;
            using iterator_concept [[maybe_unused]] = std::contiguous_iterator_tag;

            iterator() { _ptr = nullptr; };
            iterator(pointer p) : _ptr(p) {}

            reference operator*() const
            {
                if (_ptr == nullptr)
                    throw std::out_of_range("Iterator is out of range");
                return *_ptr;
            }
            pointer operator->() const
            {
                if (_ptr == nullptr)
                    throw std::out_of_range("Iterator is out of range");
                return _ptr;
            }

            iterator &operator++()
            {
                if (_ptr == nullptr)
                    return *this;
                _ptr = _ptr->next();
                return *this;
            }
            iterator operator++(int)
            {
                if (_ptr == nullptr)
                    return *this;
                iterator tmp = *this;
                _ptr = _ptr->next();
                return tmp;
            }
            iterator &operator+=(int i)
            {
                if (_ptr == nullptr)
                    return *this;
                _ptr = _ptr->traverse(i);
                return *this;
            }
            iterator operator+(const difference_type other) const
            {
                if (_ptr == nullptr)
                    return *this;
                return _ptr->traverse(other);
            }

            friend iterator operator+(const difference_type value,
                                      const iterator &other)
            {
                if (other._ptr == nullptr)
                    return other;
                return other._ptr->traverse(value);
            }

            iterator &operator--()
            {
                if (_ptr == nullptr)
                    return *this;
                _ptr = _ptr->prev();
                return *this;
            }
            iterator operator--(int)
            {
                if (_ptr == nullptr)
                    return *this;
                iterator tmp = *this;
                _ptr = _ptr->prev();
                return tmp;
            }
            iterator &operator-=(int i)
            {
                if (_ptr == nullptr)
                    return *this;
                _ptr = _ptr->traverse(-i);
                return *this;
            }
            difference_type operator-(const iterator &other) const
            {
                if (_ptr == nullptr)
                    return 0;
                return -_ptr->distance(other._ptr);
            }
            iterator operator-(const difference_type value) const
            { 
                if (_ptr == nullptr)
                    return *this;
                return _ptr->traverse(-value);
            }

            friend iterator operator-(const difference_type value,
                                      const iterator &other)
            {
                if (other._ptr == nullptr)
                    return other;
                return other._ptr->traverse(-value);
            }

            reference operator[](difference_type idx) const
            {
                if (_ptr == nullptr)
                    throw std::out_of_range("Iterator is out of range");

                auto ptr = _ptr->traverse(idx);
                return *ptr;
            }

            bool operator==(const iterator &other) const { return _ptr == other._ptr; }

        private:
            pointer _ptr;
            pointer _start;
            pointer _end;
        };

    private:
        jts_element *_raw_ptr = nullptr;
        size_t _length = 0;
        size_t _allocated = 0;
        size_t _raw_length = 0;

    public:
        iterator begin() { return iterator(_raw_ptr); }
        iterator end() { return iterator(_raw_ptr + _raw_length); }

    private:
        size_t utf8len(jts_element *ptr) const
        {
            size_t result = 0;
            for (; ptr->to_unicode(); ptr = ptr->next())
                result++;
            return result;
        }

        uint32_t to_utf8(uint32_t value, char *buffer) const
        {
            char tmp_buffer[4];
            if (buffer == nullptr)
            {
                buffer = tmp_buffer; // result is discarded
            }
            if (value < 0x80)
            {
                buffer[0] = value & 0x7f;
                return 1;
            }
            else if (value < 0x800)
            {
                buffer[0] = 0xc0 | (value >> 6);
                buffer[1] = 0x80 | (value & 0x3f);
                return 2;
            }
            else if (value < 0x10000)
            {
                buffer[0] = 0xe0 | (value >> 12);
                buffer[1] = 0x80 | ((value >> 6) & 0x3f);
                buffer[2] = 0x80 | (value & 0x3f);
                return 3;
            }
            else if (value < 0x200000)
            {
                buffer[0] = 0xf0 | (value >> 18);
                buffer[1] = 0x80 | ((value >> 12) & 0x3f);
                buffer[2] = 0x80 | ((value >> 6) & 0x3f);
                buffer[3] = 0x80 | (value & 0x3f);
                return 4;
            }
            return 0;
        }

        jts *replace_ptr(jts_element *ptr, uint32_t value)
        {
            char buffer[4];
            uint32_t val2;

            if (ptr == nullptr)
            {
                return this;
            }
            size_t len = to_utf8(value, buffer);
            size_t len2 = ptr->utf8char(&val2);
            if (len2 == len)
            {
                memcpy(ptr, buffer, len);
            }
            else if (len2 > len)
            {
                // New is shorter than old
                memmove(ptr + len, ptr + len2, _raw_length - (ptr - _raw_ptr));
                memcpy(ptr, buffer, len);
            }
            else
            {
                // New is longer than old
                size_t new_len = _raw_length + len - len2;
                if (new_len + 1 > _allocated)
                {
                    char *new_ptr = new char[new_len + 1];
                    memcpy(new_ptr, _raw_ptr, _raw_length);
                    delete[] _raw_ptr;
                    auto ptr_index = ptr - _raw_ptr;
                    _raw_ptr = (jts_element *)new_ptr;
                    _allocated = new_len + 1;
                    ptr = _raw_ptr + ptr_index;
                }
                memmove(ptr + len, ptr + len2, _raw_length - (ptr - _raw_ptr));
                memcpy(ptr, buffer, len);
                _raw_length += len - len2;
            }
            return this;
        }

    public:
        jts() = default;
        jts(const char *source)
        {
            if (source == nullptr)
            {
                _raw_ptr = nullptr;
                _length = 0;
                _allocated = 0;
                _raw_length = 0;
                return;
            }
            _raw_length = strlen(source);
            _raw_ptr = new jts_element[_raw_length + 1];
            _allocated = _raw_length + 1;
            strcpy((char *)_raw_ptr, source);
            _length = utf8len(_raw_ptr);
        }
        jts(const string &source)
        {
            _raw_ptr = new jts_element[source.length() + 1];
            _allocated = source.length() + 1;
            _raw_length = source.length();
            strcpy((char *)_raw_ptr, source.c_str());
            _length = utf8len(_raw_ptr);
        }
        jts(uint32_t unicode)
        {
            auto size = to_utf8(unicode, nullptr);
            _raw_ptr = new jts_element[size + 1];
            to_utf8(unicode, (char *)_raw_ptr);
            _raw_ptr[size] = jts_element(0);
            _length = 1;
            _allocated = size + 1;
            _raw_length = size;
        }

        ~jts()
        {
            if (_raw_ptr)
            {
                delete[] _raw_ptr;
            }
        }

    public:
        static const std::size_t npos = 0xffffffff;
        static const std::size_t nchar = 0xffffffff;

    public:
        size_t length() const { return _length; }
        size_t size() const { return _raw_length; }
        uint32_t at(size_t index) const
        {
            uint32_t result;
            jts_element *ptr = _raw_ptr;
            if (!ptr || index >= _length)
            {
                return nchar;
            }
            for (size_t i = 0; i < index; i++)
            {
                ptr = ptr->next();
            }
            result = ptr->to_unicode();
            return result;
        }
        jts *set(size_t index, uint32_t value)
        {
            jts_element *ptr = _raw_ptr;
            for (size_t i = 0; i < index; i++)
            {
                ptr = ptr->next();
            }
            return replace_ptr(ptr, value);
        }

        const char *c_str() const { return (const char *)_raw_ptr; }
        bool is_null() const { return _raw_ptr == nullptr; }

    public:
        uint32_t operator[](size_t index) const { return at(index); }
        bool operator==(const jts &str) const
        {
            if (_raw_ptr == nullptr && str._raw_ptr == nullptr)
                return true;
            if (_raw_ptr == nullptr || str._raw_ptr == nullptr)
                return false;
            return strcmp((char *)_raw_ptr, (char *)str._raw_ptr) == 0;
        }
        bool operator==(const char *str) const
        {
            if (_raw_ptr == nullptr)
                return str == nullptr;
            return strcmp((char *)_raw_ptr, str) == 0;
        }

        jts &operator=(const jts &str)
        {
            if (_raw_ptr)
                delete[] _raw_ptr;
            size_t len = str.length();
            _raw_ptr = new jts_element[len + 1];
            _allocated = len + 1;
            _raw_length = len;
            strcpy((char *)_raw_ptr, str.c_str());
            _length = str.length();
            return *this;
        }
        jts &operator=(const char *str)
        {
            if (_raw_ptr)
                delete[] _raw_ptr;
            if (str == nullptr)
            {
                _raw_ptr = nullptr;
                _length = 0;
                _allocated = 0;
                _raw_length = 0;
                return *this;
            }
            size_t len = strlen(str);
            _raw_ptr = new jts_element[len + 1];
            _allocated = len + 1;
            _raw_length = len;
            strcpy((char *)_raw_ptr, str);
            _length = utf8len(_raw_ptr);
            return *this;
        }
        jts &operator=(const string &str)
        {
            return operator=(str.c_str());
        }

        friend jts operator+(const jts &lhs, const jts &rhs)
        {
            auto len = lhs._raw_length + rhs._raw_length;
            char *buffer = new char[len];
            strcpy((char *)buffer, lhs.c_str());
            strcat(buffer, rhs.c_str());
            auto result = jts(buffer);
            delete[] buffer;
            return result;
        }
    };
}

inline ostream &operator<<(ostream &os, jts::jts const &s)
{
    if (s.is_null())
        return os;
    return os << s.c_str();
}
