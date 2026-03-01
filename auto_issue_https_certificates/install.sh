# 第一步：安装 Certbot 和 Nginx 插件
sudo apt update
sudo apt install certbot python3-certbot-nginx

# CentOS
sudo yum install certbot python2-certbot-nginx

# 第二步：一条命令完成证书申请和配置
# 将域名换成自己的
sudo certbot --nginx -d desyang.top -d www.desyang.top