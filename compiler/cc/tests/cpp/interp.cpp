// A small interpreter as a larger program: a tokenizer, a recursive
// descent parser building a tree of virtual nodes, variables in a map,
// functions in a map, strings and numbers as values, reading its script
// from stdin (interp.stdin).
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cctype>
using namespace std;

enum TokenKind { T_NUMBER, T_STRING, T_NAME, T_PUNCT, T_END };

struct Token {
    TokenKind kind;
    string text;
    double number;
    Token() : kind(T_END), number(0) {}
    Token(TokenKind k, const string &t) : kind(k), text(t), number(0) {}
};

class Lexer {
public:
    Lexer(const string &source) : src(source), at(0) {}
    vector<Token> all() {
        vector<Token> out;
        while (true) {
            Token t = next();
            out.push_back(t);
            if (t.kind == T_END) break;
        }
        return out;
    }
private:
    string src;
    size_t at;
    Token next() {
        while (at < src.size() && isspace(src[at])) at++;
        if (at >= src.size()) return Token();
        char c = src[at];
        if (isdigit(c)) {
            size_t start = at;
            while (at < src.size() && (isdigit(src[at]) || src[at] == '.')) at++;
            Token t(T_NUMBER, src.substr(start, at - start));
            t.number = atof(t.text.c_str());
            return t;
        }
        if (isalpha(c) || c == '_') {
            size_t start = at;
            while (at < src.size() && (isalnum(src[at]) || src[at] == '_')) at++;
            return Token(T_NAME, src.substr(start, at - start));
        }
        if (c == '"') {
            size_t start = ++at;
            while (at < src.size() && src[at] != '"') at++;
            string text = src.substr(start, at - start);
            at++;
            return Token(T_STRING, text);
        }
        string two = src.substr(at, 2);
        if (two == "==" || two == "!=" || two == "<=" || two == ">=") {
            at += 2;
            return Token(T_PUNCT, two);
        }
        at++;
        return Token(T_PUNCT, string(1, c));
    }
};

struct Value {
    bool isString;
    double number;
    string text;
    Value() : isString(false), number(0) {}
    Value(double n) : isString(false), number(n) {}
    Value(const string &s) : isString(true), number(0), text(s) {}
    string show() const {
        if (isString) return text;
        char buffer[64];
        if (number == (long)number) snprintf(buffer, sizeof buffer, "%ld", (long)number);
        else snprintf(buffer, sizeof buffer, "%g", number);
        return string(buffer);
    }
    bool truth() const { return isString ? !text.empty() : number != 0; }
};

struct Function;

struct Env {
    map<string, Value> vars;
    map<string, Function *> funcs;
    Env *parent;
    Env(Env *p = 0) : parent(p) {}
    Value *lookup(const string &name) {
        for (Env *e = this; e; e = e->parent) {
            map<string, Value>::iterator it = e->vars.find(name);
            if (it != e->vars.end()) return &it->second;
        }
        return 0;
    }
    Function *function(const string &name) {
        for (Env *e = this; e; e = e->parent) {
            map<string, Function *>::iterator it = e->funcs.find(name);
            if (it != e->funcs.end()) return it->second;
        }
        return 0;
    }
};

struct Node {
    virtual ~Node() {}
    virtual Value eval(Env &env) = 0;
    virtual void exec(Env &env) { eval(env); }
};

static int nodesAlive = 0;

struct Literal : Node {
    Value value;
    Literal(const Value &v) : value(v) { nodesAlive++; }
    ~Literal() { nodesAlive--; }
    Value eval(Env &) { return value; }
};

struct Variable : Node {
    string name;
    Variable(const string &n) : name(n) { nodesAlive++; }
    ~Variable() { nodesAlive--; }
    Value eval(Env &env) {
        Value *v = env.lookup(name);
        if (!v) { cout << "undefined " << name << endl; return Value(); }
        return *v;
    }
};

struct Binary : Node {
    string op;
    Node *left, *right;
    Binary(const string &o, Node *l, Node *r) : op(o), left(l), right(r) { nodesAlive++; }
    ~Binary() { delete left; delete right; nodesAlive--; }
    Value eval(Env &env) {
        Value a = left->eval(env);
        if (op == "and") return a.truth() ? right->eval(env) : a;
        if (op == "or") return a.truth() ? a : right->eval(env);
        Value b = right->eval(env);
        if (op == "+") {
            if (a.isString || b.isString) return Value(a.show() + b.show());
            return Value(a.number + b.number);
        }
        if (op == "-") return Value(a.number - b.number);
        if (op == "*") return Value(a.number * b.number);
        if (op == "/") return Value(b.number == 0 ? 0.0 : a.number / b.number);
        if (op == "%") return Value((double)((long)a.number % (long)b.number));
        if (op == "<") return Value(a.number < b.number ? 1.0 : 0.0);
        if (op == ">") return Value(a.number > b.number ? 1.0 : 0.0);
        if (op == "<=") return Value(a.number <= b.number ? 1.0 : 0.0);
        if (op == ">=") return Value(a.number >= b.number ? 1.0 : 0.0);
        if (op == "==") return Value((a.isString || b.isString ? a.show() == b.show() : a.number == b.number) ? 1.0 : 0.0);
        if (op == "!=") return Value((a.isString || b.isString ? a.show() != b.show() : a.number != b.number) ? 1.0 : 0.0);
        cout << "bad operator " << op << endl;
        return Value();
    }
};

struct Assign : Node {
    string name;
    Node *value;
    Assign(const string &n, Node *v) : name(n), value(v) { nodesAlive++; }
    ~Assign() { delete value; nodesAlive--; }
    Value eval(Env &env) {
        Value v = value->eval(env);
        Value *slot = env.lookup(name);
        if (slot) *slot = v; else env.vars[name] = v;
        return v;
    }
};

struct Block : Node {
    vector<Node *> body;
    Block() { nodesAlive++; }
    ~Block() { for (size_t i = 0; i < body.size(); i++) delete body[i]; nodesAlive--; }
    Value eval(Env &env) {
        Value last;
        for (Node *n : body) last = n->eval(env);
        return last;
    }
};

struct If : Node {
    Node *cond, *then, *otherwise;
    If(Node *c, Node *t, Node *o) : cond(c), then(t), otherwise(o) { nodesAlive++; }
    ~If() { delete cond; delete then; delete otherwise; nodesAlive--; }
    Value eval(Env &env) {
        if (cond->eval(env).truth()) return then->eval(env);
        if (otherwise) return otherwise->eval(env);
        return Value();
    }
};

struct While : Node {
    Node *cond, *body;
    While(Node *c, Node *b) : cond(c), body(b) { nodesAlive++; }
    ~While() { delete cond; delete body; nodesAlive--; }
    Value eval(Env &env) {
        int rounds = 0;
        while (cond->eval(env).truth() && rounds < 10000) { body->eval(env); rounds++; }
        return Value((double)rounds);
    }
};

struct Print : Node {
    vector<Node *> args;
    ~Print() { for (Node *n : args) delete n; nodesAlive--; }
    Print() { nodesAlive++; }
    Value eval(Env &env) {
        for (size_t i = 0; i < args.size(); i++) {
            if (i) cout << ' ';
            cout << args[i]->eval(env).show();
        }
        cout << endl;
        return Value();
    }
};

struct Function {
    vector<string> params;
    Node *body;
    Function() : body(0) {}
    ~Function() { delete body; }
};

struct Call : Node {
    string name;
    vector<Node *> args;
    Call(const string &n) : name(n) { nodesAlive++; }
    ~Call() { for (Node *n : args) delete n; nodesAlive--; }
    Value eval(Env &env) {
        if (name == "len") return Value((double)args[0]->eval(env).show().size());
        if (name == "str") return Value(args[0]->eval(env).show());
        Function *f = env.function(name);
        if (!f) { cout << "no function " << name << endl; return Value(); }
        Env local(&env);
        for (size_t i = 0; i < f->params.size() && i < args.size(); i++) local.vars[f->params[i]] = args[i]->eval(env);
        return f->body->eval(local);
    }
};

struct Def : Node {
    string name;
    Function *function;
    Def(const string &n, Function *f) : name(n), function(f) { nodesAlive++; }
    ~Def() { nodesAlive--; }
    Value eval(Env &env) {
        env.funcs[name] = function;
        return Value();
    }
};

class Parser {
public:
    Parser(const vector<Token> &t) : tokens(t), at(0) {}
    Block *program() {
        Block *b = new Block;
        while (peek().kind != T_END) b->body.push_back(statement());
        return b;
    }
    vector<Function *> functions;
private:
    vector<Token> tokens;
    size_t at;
    const Token &peek() { return tokens[at]; }
    Token take() { return tokens[at++]; }
    bool is(const string &text) { return peek().kind != T_END && peek().kind != T_NUMBER && peek().text == text; }
    bool accept(const string &text) { if (is(text)) { at++; return true; } return false; }
    void expect(const string &text) { if (!accept(text)) cout << "expected " << text << " at " << peek().text << endl; }
    Node *statement() {
        if (accept("print")) {
            Print *p = new Print;
            if (!is(";")) {
                p->args.push_back(expression());
                while (accept(",")) p->args.push_back(expression());
            }
            expect(";");
            return p;
        }
        if (accept("if")) {
            Node *c = expression();
            Node *t = block();
            Node *o = 0;
            if (accept("else")) o = is("if") ? statement() : block();
            return new If(c, t, o);
        }
        if (accept("while")) {
            Node *c = expression();
            return new While(c, block());
        }
        if (accept("def")) {
            string name = take().text;
            Function *f = new Function;
            functions.push_back(f);
            expect("(");
            if (!is(")")) {
                f->params.push_back(take().text);
                while (accept(",")) f->params.push_back(take().text);
            }
            expect(")");
            f->body = block();
            return new Def(name, f);
        }
        Node *e = expression();
        expect(";");
        return e;
    }
    Block *block() {
        expect("{");
        Block *b = new Block;
        while (!is("}") && peek().kind != T_END) b->body.push_back(statement());
        expect("}");
        return b;
    }
    Node *expression() {
        if (peek().kind == T_NAME && at + 1 < tokens.size() && tokens[at + 1].kind == T_PUNCT && tokens[at + 1].text == "=") {
            string name = take().text;
            take();
            return new Assign(name, expression());
        }
        return logical();
    }
    Node *logical() {
        Node *n = comparison();
        while (is("and") || is("or")) {
            string op = take().text;
            n = new Binary(op, n, comparison());
        }
        return n;
    }
    Node *comparison() {
        Node *n = additive();
        while (is("<") || is(">") || is("<=") || is(">=") || is("==") || is("!=")) {
            string op = take().text;
            n = new Binary(op, n, additive());
        }
        return n;
    }
    Node *additive() {
        Node *n = term();
        while (is("+") || is("-")) {
            string op = take().text;
            n = new Binary(op, n, term());
        }
        return n;
    }
    Node *term() {
        Node *n = primary();
        while (is("*") || is("/") || is("%")) {
            string op = take().text;
            n = new Binary(op, n, primary());
        }
        return n;
    }
    Node *primary() {
        Token t = take();
        if (t.kind == T_NUMBER) return new Literal(Value(t.number));
        if (t.kind == T_STRING) return new Literal(Value(t.text));
        if (t.kind == T_NAME) {
            if (accept("(")) {
                Call *c = new Call(t.text);
                if (!is(")")) {
                    c->args.push_back(expression());
                    while (accept(",")) c->args.push_back(expression());
                }
                expect(")");
                return c;
            }
            return new Variable(t.text);
        }
        if (t.text == "(") {
            Node *e = expression();
            expect(")");
            return e;
        }
        if (t.text == "-") return new Binary("-", new Literal(Value(0.0)), primary());
        cout << "unexpected " << t.text << endl;
        return new Literal(Value());
    }
};

int main() {
    string source, line;
    while (getline(cin, line)) source += line + "\n";
    Lexer lexer(source);
    vector<Token> tokens = lexer.all();
    cout << "tokens: " << tokens.size() << endl;
    Parser parser(tokens);
    Block *program = parser.program();
    cout << "nodes: " << nodesAlive << endl;
    Env global;
    program->eval(global);
    cout << "variables:";
    for (map<string, Value>::iterator it = global.vars.begin(); it != global.vars.end(); ++it) cout << ' ' << it->first << '=' << it->second.show();
    cout << endl;
    delete program;
    for (Function *f : parser.functions) delete f;
    cout << "nodes after: " << nodesAlive << endl;
    return 0;
}
