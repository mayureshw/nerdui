#ifndef _HTML_FORMATTER_H_
#define _HTML_FORMATTER_H_

#include <list>

using namespace std;

class HtmlFormatter
{
    ostream& _os;
public:
    void text(string_view s) { _os << s; }
    template <typename... Args> void text(Args&&... args)
    { (_os << ... << std::forward<Args>(args)); }
    void nl() { _os << "\n"; }
    void br() { _os << "<br>\n"; }
    void li_open() { _os << "<li>"; }
    void li_close() { _os << "</li>"; }
    void ul_open() { _os << "<ul>"; }
    void ul_close() { _os << "</ul>"; }
    void p_open() { _os << "<p>"; }
    void p_close() { _os << "</p>"; }
    template <typename... Args> void p(Args&&... args)
    { p_open(); text(std::forward<Args>(args)...); p_close(); }
    void select_open(string_view name) { _os << "<select name=\"" << name << "\">"; }
    void select_close() { _os << "</select>"; }
    void option(string_view code, string_view s)
    { _os << "<option value=\"" << code << "\">" << s << "</option>"; }
    void write_classes(list<string_view> classes)
    {
        _os << " class=\"";
        for(auto c:classes) _os << c << " ";
    }
    void span(string_view text)
    {
        _os << "<span>"
            << text
            << "</span>";
    }
    void radio(string_view name, string_view code, string_view s)
    {
        _os << "<label>"
            << "<input type=\"radio\" name=\""
            << name
            << "\" value=\""
            << code
            << "\"> "
            << s
            << "</label>";
        br();
    }
    void textinput(string_view label, string_view name)
    {
        tag_open(kwd_label,{kwd_field});
        span(label);
        _os << "<input type=\"text\" name=\""
            << name
            << "\"> ";
        tag_close(kwd_label);
        br();
    }
    void button(string_view name, string_view code, string_view descr)
    {
        _os << "<button type=\"submit\" name=\""
            << name
            << "\" value=\""
            << code
            << "\">"
            << descr
            << "</button>";
    }
    void tag_open(string_view tag, list<string_view> classes={})
    {
        _os << "<" << tag;
        write_classes(classes);
        _os << "\">";
    }
    void tag_close(string_view tag) { _os << "</" << tag << ">"; }
    HtmlFormatter(ostream& os) : _os(os) {}
};

#endif
