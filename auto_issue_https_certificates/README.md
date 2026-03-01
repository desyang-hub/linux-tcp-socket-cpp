## Nginx 服务器上部署网站，并自动签发https证书

这里默认使用Nginx作为网站服务器，其他服务器类似。

### 遇到的问题

- 找不到一个包含 server_name yourdomain.com 的 server block 

vim /etc/nginx/nginx.conf
如果看到类似 server_name _; 或没有你的域名，需要修改为
server_name desyang.top www.desyang.top;

```bash
# 测试配置并重新加载
nginx -t
systemctl reload nginx

# 重新运行certbot
sudo certbot --nginx -d desyang.top -d www.desyang.top
```

验证HTTPS
```bash
# 检查端口监听
netstat -tlnp | grep 443

# 用 curl 测试
curl -I https://desyang.top
```