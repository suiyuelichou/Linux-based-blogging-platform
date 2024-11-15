// 通过 AJAX 给服务器发请求，获取到所有的博客数据，并构造到页面上
function getBlogs() {
    $.ajax({
        type: 'get',
        url: '0',
        success: function (body) {
            // 根据返回的数据，构造出页面中对应的元素
            let containerRight = document.querySelector(".container-right");
            containerRight.innerHTML = ''; // 清空旧内容
            for (const blog of body) {
                let blogDiv = document.createElement("div");
                blogDiv.className = 'blog';

                let titleDiv = document.createElement("div");
                titleDiv.className = 'title';
                titleDiv.innerHTML = blog.title;

                let dataDiv = document.createElement("div");
                dataDiv.className = 'date';
                dataDiv.innerHTML = blog.postTime;

                let descDiv = document.createElement("div");
                descDiv.className = 'desc';
                descDiv.innerHTML = blog.content;

                let a = document.createElement("a");
                a.href = 'blog_detail.html?blogId=' + blog.blogId;
                a.innerHTML = "查看全文 &gt;&gt;";

                // 组合标签
                blogDiv.appendChild(titleDiv);
                blogDiv.appendChild(dataDiv);
                blogDiv.appendChild(descDiv);
                blogDiv.appendChild(a);
                containerRight.appendChild(blogDiv);
            }
        },
        error: function (xhr, status, error) {
            console.error('请求失败:', status, error);
        }
    });
}

// 初始化加载博客数据
getBlogs();