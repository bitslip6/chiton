scp -rp deploy_arm64/* 192.168.1.103:/usr/local
ssh root@192.168.1.103 systemctl restart chiton

