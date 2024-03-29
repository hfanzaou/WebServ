#include "Response.hpp"
#include <unistd.h>

Response::Response(request& req, const ServerConfig& config) : _req(req), _config(config), _method(req.get_method()),
	_headers_status(false), _body_status(false), auto_index(false), _fd(req.get_fd()) , Allow_method(false), redirect(false), _cgi(false), _dir(NULL)
{
	req_uri = _req.get_uri();
	status_code = _req.get_status_code();
	c = 0;
	this->_res = "";
}

Response::~Response() {}

const  ServerConfig&	Response::get_servconfig() const
{
	return (this->_config);
}
size_t Response::get_len() const
{
	return (this->_content_length);
}

const std::string& Response::get_head() const
{
	return (this->_head);
}


std::string& Response::get_body()
{
			return (this->_body);
}

std::string& Response::get_res()
{
	return (this->_res);
}

bool		Response::get_send_status()
{
	return (this->_body_status);
}

int 		Response::get_code()
{
	return (this->status_code);
}

void	Response::handle_err(int err)
{
	const std::map<short, std::string>& err_pages = _config.getErrorPages();
	if (err_pages.find(err) != err_pages.end())
	{
		this->req_uri = err_pages.find(err)->second;
		_file.open(this->req_uri, std::ifstream::binary | std::ifstream::ate);
		if (_file.is_open())
		{
			this->_content_length = _file.tellg();
			_file.seekg(0, std::ios::beg);
			std::vector<char> vec((int)this->_content_length);
			_file.read(&vec[0], (int)this->_content_length);
			std::string res(vec.begin(), vec.end());
			this->_res = set_head() + "\r\n" + res;
		}
		return;
	}
	std::string error(generate_error(err));
	if (err == 301 || err == 302)
	{
		this->_head = "HTTP/1.1 " + error + "\r\nLocation: " + this->req_uri;
		this->_res = this->_head;
		if (!redirect)
			this->_res += "/";
		return ;
	}
	if (err == 201 || err == 204)
	{
		this->_head = "HTTP/1.1 " + error;
		this->_res = this->_head + "\r\n\r\n";
		return;
	}
	std::string error_page = "<!DOCTYPE html><html><body><center><h1>ERROR</h1><h1><font size=\"100\">" 
	+ error.substr(0, 3) + "</font></h1><h1>" + error.substr(3, error.length()) + "</h1></center></body></html>";
	this->_head = "HTTP/1.1 " + error.substr(0, 3) + "\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(error_page.size()) + "\r\n\r\n" + error_page;
	this->_res = this->_head;
}

void	Response::check_method(const std::vector<std::string>& methods)
{
	for (std::vector<std::string>::const_iterator it2 = methods.cbegin(); it2 != methods.cend(); ++it2)
	{
		if (this->_method == *it2)
		{
			this->Allow_method = true;
			return ;
		}
	}
	if (this->_method != "POST" && this->_method != "DELETE" && this->_method != "GET")
		throw 501;
	throw 405;
}

void	Response::index_dir(DIR *dir, std::string& path)
{
	std::ofstream file((path + "0index.html").c_str());
	if (!file.is_open())
	{
		closedir(dir);
		throw 500;
	}
	struct dirent *reader;
	file << "<!DOCTYPE html><html><head>";
	file << "<meta charset=\"UTF-8\"><link rel=\"shortcut icon\" href=\"./.favicon.ico\">";
	file << "<title>Directory Contents</title><link rel=\"stylesheet\" href=\"/style.css\"></head><body>";
	file << "<h1 align=\"left\">" + _req.get_uri() + "</h1><div id = \"container\"><table>";
	file << "<thead><tr><th>Filename</th><th>Size</th><th>Date Modified</th></tr></thead><tbody>";
	std::string name;
	std::string size;
	std::string date = "";
	while ((reader = readdir(dir)))
	{
		name = reader->d_name;
		if (name == "0index.html")
			continue;
		file << "<tr><td><a href=\"" + name + "\">" + name;
		DIR *dir;	
		if ((dir = opendir((path + name).c_str())))
		{
			file << "/";
			size = "--";
			closedir(dir);
		}
		else
		{
			std::ifstream file_size((path + name).c_str(), std::ifstream::ate);
			if (file_size.tellg() > 1000000)
			{
				std::string temp = std::to_string((float)(file_size.tellg() / 1000000.00));
				size = temp.substr(0, temp.length() - 4) + " MB";
			}
			else 
			{
				std::string temp = std::to_string((float)(file_size.tellg() / 1000.00));
				size = temp.substr(0, temp.length() - 4) + " KB";
			}	
			file_size.close();
			file_size.clear();
		}
		struct stat Info;
		if (stat((path + name).c_str(), &Info) == 0)
			date = std::ctime(&Info.st_mtime);
		file << "</a></td>";
		file << "<td><a>" + size + "</a></td>";
		file << "<td><a>" + date + "</a></td></tr>";	
	}	
	file << "</tbody></table></div></body></html>";
	this->req_uri = path + "0index.html";
	file.close();
}
void	Response::handle_dir(std::vector<Location>::const_iterator& it, std::string& path1)
{
	if (path1.at(path1.length() - 1) != '/')
	{
		this->req_uri = path1;
		closedir(_dir);
		throw 301;
	}
	if (it->getIndex() != "")
	{
		this->index = this->req_uri + "/" + it->getIndex();
		if (access((this->index).c_str(), F_OK) != -1)
		{
			this->req_uri = this->index;
		}
		else if (it->getAutoIndex())
		{
			this->index_dir(_dir, this->req_uri);
			this->auto_index = true;
		}	
	}
	else if (it->getAutoIndex())
	{
		this->index_dir(_dir, this->req_uri);
		this->auto_index = true;
	}
	else
	{
		closedir(_dir);
		throw 403;
	}
	closedir(_dir);
}
void	Response::match()
{
	const ServerConfig& config = Response::get_servconfig();
	const std::vector<Location>& Locations = config.getLocations();
	std::string path;
	std::string root;
	std::string temp = "";
	std::vector<Location>::const_iterator to_check;
	for (std::vector<Location>::const_iterator it = Locations.cbegin(); it != Locations.cend(); ++it)
	{
		if (it->getPath() == this->req_uri.substr(0, it->getPath().length()) && it->getPath().length() > temp.length())
		{
			temp = it->getPath();
			to_check = it;
		}
	}
	if (temp == "")
		throw 404;
	if (to_check->getRedirect() != "")
	{
		redirect = true;
		this->req_uri = to_check->getRedirect();
		throw 302;
	}		
	std::string path1 = this->req_uri;	
	root = to_check->getroot();
	path = this->req_uri.substr(temp.length(), this->req_uri.length());
	while (path.substr(0, 3) == "../")
			path = path.substr(3);
	if (access((root + path).c_str(), F_OK) == -1)
	{
		temp = Locations[0].getPath();
		root = Locations[0].getroot();
		path = this->req_uri.substr(temp.length(), this->req_uri.length());
	}
	if (access((root + path).c_str(), F_OK) == -1)
		throw 404;				
	#if DEBUG

		std::cout << "final = " << root + path << std::endl;
		std::cout << "get path = " << to_check->getPath() << std::endl;
		std::cout << "root = " << root << std::endl;
		std::cout << "path = " << path << std::endl;
		std::cout << "req sub = " << this->req_uri.substr(0, to_check->getPath().length()) << std::endl;
	#endif
		if (access((root + path).c_str(), F_OK) == -1)
			throw 404;	
		this->req_uri = root + path;
		this->check_method(to_check->getMethods());
		_dir = opendir(this->req_uri.c_str());
		if (this->_method == "DELETE")
		{
			if (_dir && path1.at(path1.length() - 1) != '/')
			{
				this->req_uri = path1;
				closedir(_dir);
				throw 301;
			}	
			return ;
		}
		if (_dir && this->_method != "POST")
			this->handle_dir(to_check, path1);
		Drop_file();
		_file.open(this->req_uri, std::ifstream::binary | std::ifstream::ate);
		if (!_file.is_open())
			throw 403;
		size_t pos = this->req_uri.find(".");
		if (pos != std::string::npos && (req_uri.substr(pos + 1) == "py" || req_uri.substr(pos + 1) == "php"))
		{
			Drop_file();
			Cgi cgi(_req, req_uri);
			status_code = cgi.execute_cgi(_req.get_post().get_file_name());
			if (status_code != 200)
				throw status_code;
			this->_content_length = cgi.getCgiResponse().length() - 29;
			_cgi = true;
			this->_head = set_head();
			this->_res = cgi.getCgiResponse();
			this->_headers_status = true;
			this->_body_status = true;
			std::remove(_req.get_post().get_file_name().c_str());
			return;
		}
		else if (_method == "POST")
		{
			if (_dir)
				closedir(_dir);
			rename(_req.get_post().get_file_name().c_str(), (req_uri + "/" + _req.get_post().get_file_name()).c_str());
			throw 201;	
		}
		else
		{
			this->_content_length = _file.tellg();
			_file.seekg(0, std::ios::beg);
			return ;	
		}		
}


void Response::generate_response()
{
	try 
	{
		if (status_code != 200)
		{
			_headers_status = true;
			_body_status = true;
			throw this->status_code;
		}
		if (_headers_status == false)
			this->match();
		if (_body_status == false)
		{
			if (this->_method == "GET")
				this->handle_get();
			else if (this->_method == "DELETE")
			{
				this->handle_delete(_dir, this->req_uri);	
				throw 204;
			}
		}
	}
	catch(int err)
	{
		if (_method == "POST" && err != 201)
			std::remove(_req.get_post().get_file_name().c_str());
		this->status_code = err;
		_req.set_status_code(err);
		_headers_status = true;
		_body_status = true;
		this->handle_err(err);
		Drop_file();
	}
	catch(std::exception &e) {}
}

std::string Response::set_head()
{
	std::string head = "";
	if (!_cgi)
	{
		head += "HTTP/1.1 " + std::to_string(this->status_code) + "\r\n";
		head += "Content-Type: " + get_type(this->req_uri) + "\r\n";
	}
	head += "Content-Length: " + std::to_string(this->_content_length) + "\r\n";
	head += "Cache-Control: no-cache\r\n";
	head += "Accept: */*\r\n";
	head += "Date: " + ft_time() + "\r\n";
	head += "Accept-Encoding: gzip, deflate, br\r\n";
#if DEBUG	
	std::cout << head << std::endl;
#endif	
	return (head);
}

void	Response::handle_delete(DIR *dir, std::string req)
{
	if (access(req.c_str(), W_OK) == -1)
		throw 403;
	else if (dir)
	{
		struct dirent *reader;
		std::string path;
		while ((reader = readdir(dir)))
		{
			if (reader->d_name[0] == '.' && (reader->d_name[1] == '.' || !reader->d_name[1]))
				continue;
			path = req + "/" + reader->d_name;	
			DIR *newdir = opendir(path.c_str());
			handle_delete(newdir, path);
		}
		closedir(dir);
		if (std::remove(req.c_str()))
			throw 403;
		return ;		
	}
	else if (std::remove(req.c_str()))
		throw 403;				
}

void	Response::handle_get()
{
	if (_headers_status == false)
	{
		c = 0;
		_headers_status = true;
		this->_head = set_head() + "\r\n";
		this->_res = this->_head;
		if (!_file.is_open())
			throw 404;
		return;
	}
	int j = 1024;
	std::ifstream::pos_type l = j;
	if (_body_status == false)
	{
		if (this->_content_length < l)
			l = this->_content_length;	
		else if (this->_content_length - c < l)
		{
			l = this->_content_length - c;
		}
		c += l;
		std::vector<char> vec((int)l);
		_file.read(&vec[0], (int)l);
		std::string res(vec.begin(), vec.end());
		this->_res = res;
		if (l < j)
		{
			this->_body_status = true;
			Drop_file();
			if (this->auto_index)
				std::remove(this->req_uri.c_str());
		}
	}	
}

void	Response::Drop_file()
{
	if (_file.is_open())
	{
		_file.close();
		_file.clear();
	}
}

