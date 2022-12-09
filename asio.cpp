#include <bits/stdc++.h>
/*
for clang user
bits/stdc++.h can be found in here
https://gist.github.com/reza-ryte-club/97c39f35dab0c45a5d924dd9e50c445f


test compile version:
- gcc 11.2.0
- clang 13.0.1

command:

- gcc 
	g++ -Wall -std=c++20  -IYOUR_ASIO_LOCATION -luring
- clang++
	clang++ -Wall -std=c++20  -IYOUR_ASIO_LOCATION -stdlib=libc++ -luring
*/

#define ASIO_HAS_FILE
#define ASIO_HAS_IO_URING
#include <asio.hpp>
using namespace asio;
using namespace std;
#ifdef __clang__
constexpr auto COMPILER_NAME="clang " __clang_version__;
#elif __GNUC__
#define _TO(arg) #arg
#define __TOO(arg) _TO(arg)
constexpr auto COMPILER_NAME="GCC " __TOO(__GNUC__);
#elif
constexpr auto COMPILER_NAME="Unknown Compiler";
#endif
struct FAST_IO{
	FAST_IO(){
		ios_base::sync_with_stdio(false);
		cin.tie(NULL);
	}
}__fast_io;
#if __cplusplus < 201402L
template<class T, class U=T>
T exchange(T& obj, U&& new_value){
    T old_value=move(obj);
    obj=forward<U>(new_value);
    return old_value;
}
#endif
#define cons(a,...) a=typename decay<decltype(a)>::type(__VA_ARGS__)
using INT=int;
#define x first
#define y second
//#define int long long
#define pb push_back
#define eb emplace_back
#define all(a) (a).begin(),(a).end()
auto &_=std::ignore;
using ll=long long;
template<class T>
using vec=vector<T>;
template<bool B,class T=void>
using enableif_t=typename enable_if<B,T>::type;

#define DEF_COULD(name,exp) \
template<class U> \
struct name{\
	template<class T>\
	constexpr static auto is(int i)->decltype(exp,true){return true;}\
	template<class T>\
	constexpr static bool is(...){return false;}\
	static const bool value=is<U>(1);\
};
#define DEF_CAN(name,exp) DEF_COULD(can##name,exp)
#define ENABLE(T,name) enableif_t<can##name<T>::value>(1)
#define ENABLEN(T,name) enableif_t<!can##name<T>::value>(1)
#define FOR_TUPLE enableif_t<i!=tuple_size<T>::value>(1)
#define END_TUPLE enableif_t<i==tuple_size<T>::value>(1)
#define FOR_TUPLET(T) enableif_t<i!=tuple_size<T>::value>(1)
#define END_TUPLET(T) enableif_t<i==tuple_size<T>::value>(1)

#define DEF_INF(name,exp)\
constexpr struct{\
	template<class T>\
	constexpr operator T()const {return numeric_limits<T>::exp();}\
} name;

DEF_CAN(Out,(cout<<*(T*)(0))) DEF_CAN(For,begin(*(T*)(0)))
DEF_INF(INF,max) DEF_INF(MINF,min)

template<size_t i,class T>
auto operator>>(istream& is,T &r)->decltype(END_TUPLE,is){
	return is;
}
template<size_t i=0,class T>
auto operator>>(istream& is,T &r)->decltype(FOR_TUPLE,is){
	is>>get<i>(r);
	return operator>> <i+1>(is,r);
}

template<size_t i,class ...Args>
auto operator>>(istream& is,const tuple<Args&...> &r)->decltype(END_TUPLET(tuple<Args&...>),is){
	return is;
}
template<size_t i=0,class ...Args>
auto operator>>(istream& is,const tuple<Args&...> &r)->decltype(FOR_TUPLET(tuple<Args&...>),is){
	is>>get<i>(r);
	return operator>> <i+1>(is,r);
}

template<class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLE(T,Out),c+1);
template<size_t i,class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLEN(T,For),END_TUPLE,c+1);
template<size_t i=0,class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLEN(T,For),FOR_TUPLE,c+1);
template<class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLEN(T,Out),ENABLE(T,For),c+1);


template<class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLE(T,Out),c+1){
	os << cv;
	while (*c!='}') c++;
	return c+1;
}
template<size_t i,class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLEN(T,For),END_TUPLE,c+1){
	return c;
}
template<size_t i,class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLEN(T,For),FOR_TUPLE,c+1){
	while (*c!='{') os << *c++;
	c=__format(os,c,get<i>(cv));
	return __format<i+1>(os,c,cv);
}
template<class T>
auto __format(ostream &os,const char *c,const T& cv)->decltype(ENABLEN(T,Out),ENABLE(T,For),c+1){
	const char *ct=c+1;
	if (cv.size()==0){
		int b=1;
		while (1){
			if (*ct=='}') b--;
			if (*ct=='{') b++;
			if (!b) break;
			ct++;
		}
	}else{
		for (auto &i:cv){
			const char *cc=c+1;
			while (*cc!='{') os << *cc++;
			cc=__format(os,cc,i);
			while (*cc!='}') os << *cc++;
			ct=cc;
		}
	}
	return ct+1;
}
void _format(ostream &os,const char *c){
	while (*c!='{'&&*c!='\0') os<< *c++;
}
template<class T,class ...Args>
void _format(ostream &os,const char *c,const T &a,const Args& ...rest){
	while (*c!='{'&&*c!='\0') os<< *c++;
	if (*c=='{') c=__format(os,c,a);
	_format(os,c,rest...);
}
template<class ...Args>
string format(const char *c,const Args& ...rest){
	ostringstream os;
	_format(os,c,rest...);
	return os.str();
}
template<class ...Args>
ostream& print(const char *c,const Args& ...rest){return _format(cout,c,rest...),cout;}
template<class ...Args>
ostream& println(const char *c,const Args& ...rest){return print(c,rest...)<<endl;}

#ifndef LOCAL
#define debug(...) cerr<<format(__VA_ARGS__)
#else
#define debug(...) cerr
#endif
template<class T,class ...Args>
struct Rtar{
	T& a;tuple<Args...> n;
	Rtar(T& a,tuple<Args...> n):a(a),n(n){}
};
template<class T,class ...Args>
Rtar<T,Args&...> rtar(T &a,Args&... rest){
	return Rtar<T,Args&...>(a,tie(rest...));
}
template<size_t i,class U,class ...Args,class T=tuple<Args&...>>
auto operator>>(istream& is,Rtar<U,Args&...> r)->decltype(END_TUPLE,is){
	return is>>r.a;
}
template<size_t i=0,class U,class ...Args,class T=tuple<Args&...>>
auto operator>>(istream& is,Rtar<U,Args&...> r)->decltype(FOR_TUPLE,is){
	r.a=typename decay<U>::type(get<i>(r.n));
	for (auto &w:r.a)
		operator>> <i+1>(is,Rtar<decltype(w),Args&...>(w,r.n));
	return is;
}
template<class T1,class T2>
bool cmin(T1 &a,const T2 b){return a>b?a=b,1:0;}
template<class T1,class T2>
bool cmax(T1 &a,const T2 b){return a<b?a=b,1:0;}
template<class T1,class T2,class ...T3>
bool cmin(T1 &a,const T2 b,const T3 ...rest){return cmin(a,b)|cmin(a,rest...);}
template<class T1,class T2,class ...T3>
bool cmax(T1 &a,const T2 b,const T3 ...rest){return cmax(a,b)|cmax(a,rest...);}
bool MULTIDATA=true;

using asio::ip::tcp;
using namespace std::filesystem;

path shared_directory;

unsigned int asciihex_to_uint(char c){
	return isdigit(c)?c-'0':toupper(c)-'A'+10;
}

namespace html{
	constexpr auto html_template=
	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">" "\r\n"
	"<html>" "\r\n"
	"<head>" "\r\n"
	"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">" "\r\n"
	"<title>Directory listing for {}</title>" "\r\n"
	"</head>" "\r\n"
	"<body>" "\r\n"
	"<h1>Directory listing for {}</h1>" "\r\n"
	"<hr>" "\r\n"
	"<ul>" "\r\n"
	"{<li><a href=\"{}/\">{}</a></li>" "\r\n}"
	"{<li><a href=\"{}\">{}</a></li>" "\r\n}"
	"</ul>"
	"</hr>"
	"</body>"
	"</html>"
	;
	
	string encode(string_view a){
		ostringstream os;
		os.fill('0');
		os<<hex;
		for (auto c:a)
		{
			if (isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')
			{
				os<<c;
			}else{
				os<<uppercase
				<<'%'<<setw(2)<< int((unsigned char)c)
				<<nouppercase;
			}
		}
		return os.str();
	}
	
	string decode(string_view a){
		string os;
		for (auto i=begin(a);i!=end(a);)
		{
			if (*i=='%'&&end(a)-i>=3)
			{
				os+=asciihex_to_uint(*next(i))*16+
					asciihex_to_uint(*next(i,2));
				i+=3;
			}else
			{
				os+=*i;
				i++;
			}
		}
		return os;
	}
}

vec<string> getfiles(path dir){
	vec<string> a;
	for (const auto& i:directory_iterator(dir))
		a.push_back(i.path().filename());
	return a;
}

string format_now(string format){
	time_t t=time(NULL);
	return (stringstream()<<put_time(gmtime(&t), format.c_str())).str();
}

template<size_t n=1>
struct match_endl{
	template<class Iterator>
	pair<Iterator,bool> operator()(Iterator begin,Iterator end){
		size_t c1 = 0 ,c2 = 0;//c1 marks "\n" c2 marks "\r\n"
	  Iterator i = begin;
	  decay_t<decltype(*i)> prev{};
	  while (i != end){
	  	if (*i=='\n') 
	  	{
	  		c1++;
	  		if (prev=='\r') c2++;
	  		else c2=0;
	  	}else
	  	{
	  		c1=0;
	  		if (*i!='\r') c2=0;
	  	}
	  	if (c1==n) return {i,true};
	  	if (c2==n) return {i,true};
	  	prev=*i++;
	  }
	  return {end, false};
	};
};

namespace asio {
  template <size_t i> struct is_match_condition<match_endl<i>>
    : public true_type {};
} // namespace asio

string_view strip(string_view view){
	while (view.size()&&isspace(view.front()))
		view.remove_prefix(1);
	while (view.size()&&isspace(view.back()))
		view.remove_suffix(1);
	return view;
}

struct http_header_view
{
	template<class StringType>
	http_header_view(StringType&& raw):
		raw_(forward<StringType>(raw))
	{
		auto i=raw_.begin();
		{
			auto k=match_endl()(i,raw_.end()).first-i;
			auto j1=string_view(&*i,k).find(' ');
			auto j2=string_view(&*(i+j1+1),k-j1-1).find(' ');
			method=strip({&*i,j1});
			url=strip({&*i+j1+1,j2});
			version=strip({&*i+j1+1+j2+1,k-j1-j2-2});
			i+=k;
		}
		while (i!=raw_.end())
		{
			auto k=match_endl()(i,raw_.end()).first-i;
			if (k>1)
			{
				auto j=string_view(&*i,k).find(':');
				if (j!=string_view::npos&&j>0&&k-j-1>0)
				{
					kv_[strip({&*i,j})]=strip({&*i+j+1,k-j-1});
				}
			}
			i+=k;
			if (i!=raw_.end()) i++;
		}
	}
	string raw_;
	string_view method,url,version;
	map<string_view,string_view> kv_;
};

namespace http{
	
	map<int,string> status_code_to_name=
	{
		{200,"OK"},
	};
	
	enum data_type
	{
		html,
		file,
	};
	
	map<data_type,string> data_type_name=
	{
		{html,"text/html; charset=utf-8"},
		{file,"application/octet-stream"}
	};
	
	string make_http_header(
		int status_code,
		size_t size,
		data_type type,
		vec<string> extra={})
	{
			string ans;
			auto add_line=[&ans](string str){ans+=move(str)+"\r\n";};
			add_line("HTTP/1.0 "+to_string(status_code)+" "+
			status_code_to_name[status_code]);
			add_line("Server: "s+"SimpleHTTP/0.1 "+COMPILER_NAME);
			add_line("Date: "+format_now("%c %Z"));
			add_line("Content-Length: "+to_string(size));
			add_line("Content-type: "+data_type_name[type]);
			for (auto& i:extra) add_line(i);
			add_line("");
			return ans;
	}
	
}



struct http_seesion
{
	
	http_seesion(tcp::socket socket,io_context& ctx):
		socket_(move(socket)),ctx_(ctx){}
	~http_seesion()
	{
		socket_.close();
	}
	
	awaitable<string> read_headers()
	{
		string header;
		header.resize(
			co_await async_read_until(
				socket_,
				dynamic_buffer(header),
				match_endl<2>(),
			use_awaitable));
		co_return header;
	}
	
	awaitable<void> write_html(string context,int status_code=200)
	{
		co_await async_write(socket_,
			buffer(make_http_header(status_code,context.size(),http::html)),
			use_awaitable);
		co_await async_write(socket_,
			buffer(context),
			use_awaitable);
	}
	
	awaitable<void> write_file(path filename,int status_code=200)
	{
		size_t filesize=file_size(filename),available=0;
		co_await async_write(socket_,
			buffer(make_http_header(status_code,filesize,http::file)),
			use_awaitable);
		array<char,16*1024> data;//buffer size 16k
		auto file=stream_file(ctx_,filename,file_base::flags::read_only);
		while ((filesize-=available)&&
			(available=co_await file.async_read_some(buffer(data),use_awaitable)))
		{
			co_await async_write(socket_,buffer(data,available),use_awaitable);
		}
	}
	
	awaitable<void> work()
	{
		try
		{
			http_header_view header_view(co_await read_headers());
			
			int status_code;
			
			if (header_view.url.front()=='/') header_view.url.remove_prefix(1);
			auto dirpath = shared_directory/html::decode(header_view.url);
			
			if (is_directory(dirpath))
			{
				vec<pair<string,string>> folders={
					{".","."},
					{"..",".."}
				},files;
				
				for (auto i:directory_iterator(dirpath))
				{
					string filename=i.path().filename();
					(i.is_directory()?folders:files).push_back(
						{html::encode(filename),filename}
					);
				}
				
				co_await write_html(format(
					html::html_template,
						header_view.url,
						header_view.url,
						folders,
						files
					),status_code=200);
			}else if (is_regular_file(dirpath)){
				co_await write_file(dirpath,status_code=200);
			}else {
				co_await write_html("",status_code=404);
			}
			
			cerr<<format("[{}] \"{} /{} {} {}\"\n",
				format_now("%c"),
				header_view.method,
				header_view.url,
				header_view.version,
				status_code
			);
		}catch(const exception& e)
		{
			cerr<<e.what()<<endl;
		}
		
	}
	tcp::socket socket_;
	io_context& ctx_;
};


awaitable<void> http_server(tcp::acceptor& acceptor,io_context& ctx)
{
	for (;;)
	{
		auto client = co_await acceptor.async_accept(use_awaitable);
		auto ex = client.get_executor();
		co_spawn(ex,[c=move(client),&ctx]()mutable->awaitable<void> {
			co_await http_seesion(move(c),ctx).work();
		}()
		,detached);
	}
}

int main(int argc,char *argv[]){
	
	if (argc!=4)
	{
		cerr << "Usage: simple http file server\n";
		cerr << " <listen_address> <listen_port>\n";
		cerr << " <shared_directory>\n";
		cerr << "Version: 1.0\n";
		cerr << "Compiler: " << COMPILER_NAME << "\n";
		return 1;
	}
	
	if (!is_directory(shared_directory=argv[3]))
	{
		cerr << "Shared_directory not exist" << endl;
		return 1;
	}
	
	try
	{
		io_context ctx;
		
		auto listen_endpoint=
			*tcp::resolver(ctx).resolve(argv[1],argv[2],tcp::resolver::passive);
			
		tcp::acceptor acceptor(ctx,listen_endpoint);
		
		cerr << format("Serving HTTP on {} port {} (http://{}:{}/) ...\n",
			argv[1],argv[2],argv[1],argv[2]);
		
		co_spawn(ctx,
			http_server(acceptor,ctx),
		detached);
		
		ctx.run();
	}
	catch(exception& e)
	{
		cerr << "exception:" << e.what() << endl;
	}
}

