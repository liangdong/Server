<h1>此分支用于开发Http相关的插件, 主框架代码与主干相同.</h1>
<p>1, 开发简单的plugin cgi, 并使用一个shell脚本作为示例.</p>
<p>2, 开发简单的静态文件访问plugin.</p>
<h3>@2012/10/29 23:02: 支持CGI工作方式, 脚本需以.cgi为后缀, 具有可执行权限, 脚本头部需要#!声明解释器. 为更好的支持CGI等模块, 新加入plugin的OnTimer回调.</h2>
<h3>怎么测试CGI? 项目自带两个CGI程序, 一个是Python的, 一个是Shell的, 访问方式为:http://119.254.35.221:10001/cgi/jump.cgi 与 http://119.254.35.221:10001/cgi/echo.cgi</h3>
<h4>python脚本将打印环境变量, 环境变量由Server设置并继承, 包含了所有Http请求的信息, shell脚本则为简单的echo.</h4>

<h4>Plugin_static已支持, 访问http://119.254.35.221:10001/doc/404.html, http://119.254.35.221:10001/doc/index.html可测试结果</h4>
