#!/bin/bash

# 数据库连接信息
DB_HOST="localhost"       # 数据库地址
DB_USER="root"            # 数据库用户名
DB_PASS="bd20030423"        # 数据库密码
DB_NAME="yourdb"   # 数据库名称

# 插入博客的SQL模板
INSERT_SQL="INSERT INTO blog (title, content, userId, postTime) VALUES"

# 插入数据的逻辑
for i in $(seq 1 100); do
  # 生成测试数据
  TITLE="测试博客标题 $i"
  CONTENT="这是一篇测试博客的内容，编号为 $i。这篇文章用于测试分页功能。"
  USER_ID=$((i % 10 + 1))  # 模拟用户 ID，从 1 到 10 循环
  POST_TIME=$(date -d "-$((100 - i)) days" +"%Y-%m-%d %H:%M:%S") # 模拟过去的时间

  # 构建插入语句
  SQL="${INSERT_SQL} (\"$TITLE\", \"$CONTENT\", $USER_ID, \"$POST_TIME\");"

  # 执行SQL语句
  mysql -h "$DB_HOST" -u "$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "$SQL"

  # 输出进度
  echo "已插入博客 $i：$TITLE"
done

echo "完成插入100条测试博客数据！"
