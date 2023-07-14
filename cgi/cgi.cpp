#include "cgi.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

Cgi::Cgi(request& _req, std::string& _path) : req(_req), path(_path)
{}

char    **Cgi::getEnv()
{
    std::map<std::string, std::string>	headers = req.get_header();

    _env["HOST"] = headers["Host"];
	_env["REQUEST_METHOD"] = req.get_method();
	// if (req.get_method() == "POST")
	// {
    	_env["CONTENT_LENGTH"] = headers["Content-Length"];
		_env["CONTENT_TYPE"] = headers["Content-Type"];
	// }

    std::map<std::string, std::string>::iterator it;
	char **env = new char*[5];
	int i = 0;
	for(it = _env.begin(); it != _env.end(); it++)
    {
		std::string keyValue = it->first + "=" + it->second;
		env[i] = new char[keyValue.size() + 1];
		strcpy(const_cast<char*>(env[i]), keyValue.c_str());
		i++;
	}
	env[i] = NULL;
	return env;

}

void    Cgi::execute_cgi()
{
    int pid;
    int fd[2];

    char    **env = getEnv();
    pipe(fd);

	int fdin = open("test", O_RDONLY);

    if ((pid = fork()) == 0)
    {
        close(fd[0]);
		dup2(fdin, 0);
        dup2(fd[1], 1);
        close(fd[1]);

        execve(path.c_str(), nullptr, env);
        perror(NULL);
        std::cerr << "Error executing script" << std::endl;
    }
    else
    {
        close(fd[1]);
        waitpid(-1, NULL, 0);
        char buff[1024];
        int r = 1;
        while (r != 0)
        {
            memset(buff, 0, 1024);
            r = read(fd[0], buff, 1023);
            buff[r] = 0;
            cgi_response += buff;
        }
        close(fd[0]);
    }
    std::cout << "waaaaaaa" << cgi_response << std::endl;
}

const std::string& Cgi::getCgiResponse() const
{
    return cgi_response;
}