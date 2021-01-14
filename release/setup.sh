#!/bin/sh

echo "*		soft	core	unlimited" >> /etc/security/limits.conf

useradd -p $(openssl passwd -1 "chiton") "root"

addgroup chiton chiton

echo "%chiton ALL=(root) NOPASSWD: ALL" > /etc/sudoers.d/20-chiton.conf

systemctl restart chiton

