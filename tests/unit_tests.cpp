// Unit tests for the dependency-free infrastructure (JSON, crypto, util, router).
#include <iostream>
#include "../backend/include/json.h"
#include "../backend/include/crypto.h"
#include "../backend/include/util.h"
#include "../backend/include/http_server.h"
using namespace std;
static int failures = 0, total = 0;
#define CHECK(c) do { total++; if(!(c)) { failures++; cout << "FAIL line " << __LINE__ << ": " #c << endl; } } while(0)

int main() {
    // SHA-256 known vectors (FIPS 180-4)
    CHECK(crypto::hex(crypto::sha256(crypto::bytes("abc"))) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(crypto::hex(crypto::sha256(crypto::bytes(""))) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(crypto::hex(crypto::sha256(crypto::bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // HMAC-SHA256 RFC 4231 test case 2
    CHECK(crypto::hex(crypto::hmac(crypto::bytes("Jefe"), crypto::bytes("what do ya want for nothing?"))) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    // PBKDF2-HMAC-SHA256 vectors (RFC 7914 section 11)
    CHECK(crypto::hex(crypto::pbkdf2("passwd", crypto::bytes("salt"), 1, 32)) == "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc");
    CHECK(crypto::hex(crypto::pbkdf2("Password", crypto::bytes("NaCl"), 80000, 32)) == "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56");
    string h = crypto::hashPassword("S3cret!pass", 2000);
    CHECK(crypto::verifyPassword("S3cret!pass", h));
    CHECK(!crypto::verifyPassword("wrong", h));
    CHECK(!crypto::verifyPassword("x", "garbage"));
    CHECK(crypto::hashPassword("a", 1000) != crypto::hashPassword("a", 1000)); // random salt

    // JSON
    Json j = Json::parse("{\"a\":1,\"b\":[true,null,\"x\\n\\u00e9\\ud83d\\ude00\"],\"c\":{\"d\":-2.5e1}}");
    CHECK(j.at("a").integer() == 1);
    CHECK(j.at("b").items().size() == 3 && j.at("b").items()[0].boolean());
    CHECK(j.at("b").items()[2].str() == "x\n\xc3\xa9\xf0\x9f\x98\x80");
    CHECK(j.at("c").at("d").num() == -25.0);
    CHECK(Json::parse(j.dump()).dump() == j.dump());
    Json o = Json::object(); o["n"] = 5; o["s"] = "q\"uote"; o["f"] = 1.5; o["z"] = Json();
    CHECK(o.dump() == "{\"n\":5,\"s\":\"q\\\"uote\",\"f\":1.5,\"z\":null}");
    for (const char* bad : {"{", "[1,]", "{\"a\":}", "tru", "\"abc", "{\"a\":1} x", "[1 2]", "", "{'a':1}"}) {
        bool threw = false; try { Json::parse(bad); } catch (const exception&) { threw = true; }
        CHECK(threw);
    }
    string deep(100, '['); bool threw = false; try { Json::parse(deep); } catch (const exception&) { threw = true; } CHECK(threw);

    // util
    CHECK(util::validDate("2028-02-29") && !util::validDate("2027-02-29") && !util::validDate("2027-13-01") && !util::validDate("27-01-01"));
    CHECK(util::validEmail("a@b.co") && !util::validEmail("a@b") && !util::validEmail("@b.co") && !util::validEmail("a b@c.com"));
    CHECK(util::validPhone("+91-98765-43210") && !util::validPhone("12ab") && !util::validPhone("123"));
    double d = util::haversineKm(30.3165, 78.0322, 28.6139, 77.2090); // Dehradun - Delhi ~ 215 km (straight line)
    CHECK(d > 190 && d < 240);
    CHECK(util::urlDecode("a%20b+c%2F") == "a b c/");

    // router
    Router r; string seen;
    r.get("/api/x/search", [&](HttpRequest&, HttpResponse& res) { seen = "search"; res.send(200, Json::object()); });
    r.get("/api/x/:id", [&](HttpRequest& q, HttpResponse& res) { seen = "id=" + q.param("id"); res.send(200, Json::object()); });
    HttpRequest rq; HttpResponse rs;
    rq.method = "GET"; rq.path = "/api/x/search"; r.dispatch(rq, rs); CHECK(seen == "search");
    rq.path = "/api/x/42"; r.dispatch(rq, rs); CHECK(seen == "id=42" && rs.status == 200);
    rq.path = "/api/nope"; r.dispatch(rq, rs); CHECK(rs.status == 404);
    rq.method = "POST"; rq.path = "/api/x/42"; r.dispatch(rq, rs); CHECK(rs.status == 405);
    rq.params["id"] = "12abc"; bool bad = false; try { rq.paramInt("id"); } catch (const ApiError&) { bad = true; } CHECK(bad);

    cout << (total - failures) << "/" << total << " checks passed" << endl;
    return failures ? 1 : 0;
}
