<h1>Server 一个纯异步的Server简单实现</h1>
<h2>基于Nginx/Lighttpd的状态机实现, 感兴趣的同学有福了. </h3>
<h3>****Server全新支持Http协议(支持KeepAlive)****</h3>
<h4>您可以直接访问http://119.254.35.221:10000/看一下效果</h4>

<h1>Http-Server分支用于开发Http相关的插件, 主框架代码与主干相同, 已支持CGI Plugin</h1>

<p>最新说明:</p>
<p>@2012/10/22 18:42: 代码整理划分模块, 添加plugin回调逻辑, 提供简单的示例plugin demo, 供感兴趣的同学阅读与反馈. </p>
<p>@2012/10/23 15:00: 添加Mysql Plugin, 提供了一个生动的例子, 引入多个Plugin的过程中发现了一些新问题, 因此src/client.cpp, src/server.cpp中的Plugin调用逻辑出现了变动, 在代码中有相关注释, 现在整个项目更具有可读性与实用性了, 欢迎大家找BUG提建议.</p>
<p>@2012/10/25 10:46: fake_mysql添加线程池, 更好的展现异步高并发特性.</p>
<p>@2012/10/26 16:18: src/client.cpp优化一个逻辑问题, 以便保证任何Plugin在ON_RESPONSE阶段一旦返回OK则不会被再次回调, 因此不再需要Plugin开发者自己维护回调状态, 简化了开发逻辑复杂度. (slow_query插件代码做出了对应的修改, 以便适应这个优化后的逻辑)</p>
<p>@2012/10/26  23:00 Server初步支持HTTP协议</p>
<p>@2012/10/27  10:44 Client延迟释放逻辑的已经完成, 这是一个现存的隐蔽BUG, 与多线程插件相关, 通过为Client事件回调逻辑添加一处状态检查以便等待所有插件完成工作后释放Client, 解决了该隐蔽复杂的逻辑BUG(欲知BUG细节可以联系我).</p>
<p>@2012/10/27  15:18 Client完善Http 500应答逻辑, 如果插件在ON_RESPONSE期间返回ERROR, 则向客户端返回500错误应答，为简化此实现，状态机新增BEFORE_ERROR, ON_ERROR状态以便实现500应答逻辑.</p>
<p>@2012/10/27  0:16 添加HttpResponse结构体, 替换原先std::string m_response, 添加相应序列化方法. 因此, 所有插件需调整代码, 直接操作HttpResponse结构体, 最终由主框架负责序列化后送出.</p>
<p>@2012/10/28  11:10: 代码趋向于稳定, 接下来将会主要做代码优化整理，包括添加中文注释等。</p>
<p>接下来将进行HttpResponse, HttpRequest结构体的API开发，以便让Plugin开发者可以方便的操作HTTP请求与应答，比如提供方便的获取GET参数的API，以及组织x-www-form表单POST数据拆解以便快速的访问key:val等等.</p>
<p>另外说明, Server主要是做应用逻辑的，基于HTTP协议，所以并不打算直接响应静态文件请求，所有逻辑交由plugin处理，我们是完全有能力开发一个静态文件plugin的，而且是很简单的. 并且, 接下来将会做几个有意思的插件, 比如统计Referer跳转以便统计本开源项目的关注人群是从哪个论坛跳转而来, 比如开发插件统计每个IP的访问次数以及user-agent等信息统计,再比如开发一个静态文件插件等等, 希望有同学可以在理解项目代码的基础上开发自己的插件, 提出开发过程中觉得不爽的地方, 有利于项目改进.</p>

<p>Server自带两个Plugin:<br/>
1 ) slow_query: 该plugin对于任何一个请求, 将会延迟1秒后应答, 将在body中回显Http请求.<br/>
2 ) fake_mysql: 该plugin伪装真实的Mysql请求操作, 任何请求将会阻塞20毫秒后返回Mysql查询结果(伪装一个较快的Mysql查询请求), 结果为Mysql_Query(uri)<br/>

更多插件, 期待你的加入.
</p>

<h2>How to use ?</h2>
(需要安装libevent)<br/>
1, tar -zxvf libevent.tar.gz 解压<br/>
2, make -C plugin/slow_query && make -C plugin/fake_mysql 编译两个Plugin<br/>
3, make 编译Server<br/>
4, ./server 运行<br/>

<h2>How to test ?</h2>
1.curl "http://localhost:10000/" -d "hello world" 其中-d为POST数据的内容， 也可以使用GET方法。<br/>
2.使用tools/test.py, 可以大致的测试一下服务器是否可以在并发量提升后是否可以正常工作(我拿来看会不会出core等.) <br/>
3.使用tools/test_stream.py, 测试连续HTTP请求是否可以被正常响应. <br/>

<p></p>
<h2>背景:</h2>       
      XX希望用Epoll开发Server, 但业务上有很多阻塞的逻辑, 怕会阻塞Epoll, 所以不知道怎么设计比较好, 也就是知道大概要怎样, 但实际操作上又举步维艰的一种状态.
      
      比如, 老大交给他几个任务:
      1, 需要将Server接受到的请求向其他Server做转发并从其他Server读取应答后发回给Client(像反向代理一样).
      2, 需要将Client请求存入数据库, 或者从数据库中查询一些东西后返回给Client.
      
      XX疑惑的是, 他希望用epoll单线程, 而又不希望业务逻辑阻塞线程. 他大概知道要将业务逻辑放到线程中去阻塞的处理, 但不
      知道怎么融合到epoll单线程中, 有些不知所措了.
<p>===============================================================================================================</p>
<h2>说明:</h2>
<h4>
     &nbsp;&nbsp;我隔天写了一段代码, 只写了一些必要的内容, 让XX借着代码理解一下实现方式. 其实和一般的I/O复用Server开发是基本一样的,但为了解决XX的问题, 引入状态机机制会让整个Server看起来更清晰易懂, 让Server架构和具体的业务逻辑解耦开来, 真正开发时只需要关注自己的业务逻辑, 这是非常有益处的, 同时, 也为XX解开了他最初的疑惑.
</h4>

<h2>代码核心:</h2> 
<h4>&nbsp;&nbsp;基于write事件来完成轮询检查, 将业务阻塞逻辑移到独立线程处理, 用户请求在主线程与业务线程间异步传输, 是整个框架的核心思路.
程序使用C++编写, 通过多态将Plugin解耦(和C中函数指针callback一样), 开发者最终只需开发自己的plugin so即可实现可靠的Server.
