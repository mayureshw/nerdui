#include <iomanip>
#include <memory>
#include <unordered_map>
#include "types.h"
#include <mustache.hpp>
#include "fcgibridge.h"

inline const string kwd_sessionid = "sessionid";
inline const string kwd_form = "form";

constexpr char resp[] = R"(
<!DOCTYPE html>
<html>
<head>
    <title>)" APP_TITLE R"(</title>
</head>
<body>

<form method="post" action="">
    <input type="hidden" name="sessionid" value="{{sessionid}}">
    {{{form}}}
</form>

</body>
</html>
)";

inline const size_t uid_length = 16; // bytes
class Session
{
    string _sessionid;
    Struct<DefaultSessionType>* _sessionobj;
    Response _resp;
    string make_session_id()
    {
        array<unsigned char, uid_length> buf;
        arc4random_buf(buf.data(), buf.size());
    
        ostringstream os;
        for (unsigned char b : buf)
            os << hex << setw(2) << setfill('0') << (int)b;
        return os.str();
    }
    // Tricky mustache quirk. template doesn't work as static member
    kainjow::mustache::mustache& tmpl_resp() const {
        static kainjow::mustache::mustache t{resp};
        return t;
    }
    void updateState(Query& query)
    {
        e_event_type event = query.contains(string(kwd_eid))
            ? static_cast<e_event_type>(stoi(query[string(kwd_eid)]))
            : E_SET;
        switch(event)
        {
        case E_SET:
        case E_NEXT: {
            auto dat = query.contains(string(kwd_dat))
                ? query[string(kwd_dat)] : kwd_empty;
            auto eh = _resp.eh();
            if ( eh ) eh->handleEvent(event,dat);
            break;
            }
        case E_BACK: {
            auto lasteh = _resp.last_eh();
            if ( lasteh ) {
                lasteh->handleEvent(event,kwd_empty);
                _resp.resetLast();
                }
            break;
            }
        case E_DONE: {
            // Shall we really do memory op or recycle existing object
            delete _sessionobj;
            _sessionobj = new DefaultSessionType();
            _resp.reset();
            break;
            }
        }
    }
    string _getResponse()
    {
        kainjow::mustache::data data;
        data.set(kwd_sessionid, _sessionid);
        _sessionobj->getResponse<NoneType,0>(_resp);
        _resp.buttons();
        data.set(kwd_form,_resp.str());
        _resp.clear();
        return tmpl_resp().render(data);
    }
public:
    string& id() { return _sessionid; }
    string getResponse(Query& query)
    {
        updateState(query);
        return _getResponse();
    }
    Session()
    {
        _sessionid = make_session_id();
        _sessionobj = new DefaultSessionType();
    }
    ~Session()
    {
        delete _sessionobj;
    }
};

#ifdef WASM
class SessionManager
#else
class SessionManager : public FCGIBridge
#endif
{
    unordered_map<string,Session*> _sessions;
public:
    string getResponse(Query& query)
    {
        if( query.contains(kwd_sessionid) )
        {
            auto sessionid = query[kwd_sessionid];
            auto it = _sessions.find(sessionid);
            if( it == _sessions.end() ) return "<p>invalid session</p>";
            else return it->second->getResponse(query);
        }
        else
        {
            // TODO: Until session expiry is implemented, we'll empty the sessions
            for( auto it = _sessions.begin(); it != _sessions.end(); it++ )
                delete it->second;
            _sessions.clear();
            auto session = new Session();
            _sessions.emplace( session->id(), session );
            return session->getResponse(query);
        }
    }
};
