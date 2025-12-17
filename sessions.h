#include <iomanip>
#include <memory>
#include <unordered_map>
#include "types.h"
#include "fcgibridge.h"

inline const string kwd_sessionid = "sessionid";

class Session
{
    string _sessionid;
    Struct<DefaultSessionType>* _sessionobj;
    string make_session_id()
    {
        array<unsigned char, 32> buf; // 256 bits
        arc4random_buf(buf.data(), buf.size());
    
        ostringstream os;
        for (unsigned char b : buf)
            os << hex << setw(2) << setfill('0') << (int)b;
        return os.str();
    }
public:
    string& id() { return _sessionid; }
    string getResponse(Query& query)
    {
        return "<p>valid session<p>";
    }
    Session()
    {
        _sessionid = make_session_id();
        _sessionobj = new Struct<DefaultSessionType>();
    }
    ~Session()
    {
        delete _sessionobj;
    }
};

class SessionManager : public FCGIBridge
{
    unordered_map<string,unique_ptr<Session>> _sessions;
public:
    string getResponse(Query& query)
    {
        if( query.contains(kwd_sessionid) )
        {
            auto sessionid = query[kwd_sessionid];
            auto it = _sessions.find(sessionid);
            if( it == _sessions.end() ) return "<p>invalid session<p>";
            else return it->second->getResponse(query);
        }
        else
        {
            // TODO: Until session expiry is implemented, we'll empty the sessions
            _sessions.clear(); // unique_ptr deletes the sessions
            auto session = make_unique<Session>();
            _sessions.emplace( session->id(), move(session) );
            return session->getResponse(query);
        }
    }
};
