#!bin/bash
#1. check mariadb installed
#2. git fetch & pull
#3. regist daemon service

INSTALL_MARIADB=$(dpkg -l mariadb-client 2> /dev/null | grep ii | cut -d ' ' -f 1)

echo "checking mariadb..."
if ! [ "$INSTALL_MARIADB" == "ii" ]; then
  read -s -n 1 -p "press any key to install mariadb."
  printf "\n"
  echo "install mariadb..."
  apt-get install -y mariadb-server > /dev/null
  apt-get install -y mariadb-client > /dev/null
  echo "set allow ufw rules."
  ufw allow 3306
fi

STATUS_DB=$(systemctl status mariadb | grep Active | cut -d ' ' -f 7) > /dev/null

if [ "$STATUS_DB" == "active" ]; then
    echo "db is activated."
else
    echo "Error : please typing 'systemctl status mariadb.service'"
    exit 0
fi

if ! [ -d "GAZ-AAA_CoinPrices" ]; then
  exit 0
else
  echo "no exist"
fi

CURRENT_PATH=$(pwd)
git --version

git remote -v
echo "$?"
pwd