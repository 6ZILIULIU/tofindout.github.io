# *tp框架路由共有4种形式*
2018年03月10日 11:42:02 不负好时光1001 阅读数 1544
 版权声明：本文为博主原创文章，未经博主允许不得转载。 https://blog.csdn.net/weixin_39768635/article/details/78195443
 ---
1. 基本get形式
http://网址/index.php?m=分组&c=控制器&a=操作方法
该方式是最底层的get形式、传统的参数传递方式，不时尚、不安全。
2. pathinfo路径形式[默认方式]
http://网址/index.php/分组/控制器/操作方法
http://网址/index.php/Home/Index/advert
3. rewrite重写形式(伪静态技术)省略index.php入口文件
    http://网址/分组/控制器/操作方法
    http://网址/Home/Index/index
4. 兼容形式
http://网址/index.php?s=/分组/控制器/操作方法
http://网址/index.php?s=/Home/Index/advert
