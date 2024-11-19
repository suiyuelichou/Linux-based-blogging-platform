let currentPage = 1; // 当前页
let totalPages = 1; // 总页数
const pageSize = 20; // 每页博客条数
let isLoading = false; // 是否正在加载

// 获取博客数据并更新页面
function getBlogs(page) {
    if (isLoading || page > totalPages) return; // 避免重复加载或超过总页数
    isLoading = true;

    $.ajax({
        type: 'GET',
        url: `get_blogs?page=${page}&size=${pageSize}`,
        success: function (response) {
            // 解构响应数据
            const { blogs, totalCount } = response;
            totalPages = Math.ceil(totalCount / pageSize); // 计算总页数

            // 更新页面博客列表
            const containerRight = document.querySelector(".container-right");
            // if (page === 1) containerRight.innerHTML = ''; // 首次加载清空旧内容

            for (const blog of blogs) {
                let blogDiv = document.createElement("div");
                blogDiv.className = 'blog';

                let titleDiv = document.createElement("div");
                titleDiv.className = 'title';
                titleDiv.textContent = blog.title;

                let dateDiv = document.createElement("div");
                dateDiv.className = 'date';
                dateDiv.textContent = blog.postTime;

                let descDiv = document.createElement("div");
                descDiv.className = 'desc';
                descDiv.textContent = blog.content;

                let a = document.createElement("a");
                a.href = `blog_detail_user.html?blogId=${blog.blogId}`;
                a.textContent = "查看全文 >>";

                blogDiv.appendChild(titleDiv);
                blogDiv.appendChild(dateDiv);
                blogDiv.appendChild(descDiv);
                blogDiv.appendChild(a);
                containerRight.appendChild(blogDiv);
            }

            // 更新分页状态
            updatePagination(page);
            isLoading = false;

            // 当页面内容不足时，自动加载下一页
            if (containerRight.scrollHeight <= containerRight.clientHeight && currentPage < totalPages) {
                currentPage++;
                getBlogs(currentPage);
            }
        },
        error: function (xhr, status, error) {
            console.error('请求失败:', status, error);
            isLoading = false;
        }
    });
}

// 加载指定页
function loadPage(page) {
    if (page < 1 || page > totalPages) return;
    currentPage = page;

    // 清空当前页面博客内容
    const containerRight = document.querySelector(".container-right");
    containerRight.innerHTML = ''; // 清空旧内容

    // 加载新博客数据
    getBlogs(currentPage);
}

// 更新分页按钮状态
function updatePagination(page) {
    document.getElementById("prevPage").disabled = page <= 1;
    document.getElementById("nextPage").disabled = page >= totalPages;

    const currentPageText = document.getElementById("currentPage");
    currentPageText.textContent = `第 ${page} 页`;
}

// 滚动加载事件
const containerRight = document.querySelector('.container-right');
containerRight.addEventListener('scroll', function () {
    if (isLoading || currentPage >= totalPages) return;

    // 计算滚动位置和阈值
    const scrollTop = containerRight.scrollTop; // 已经滚动的距离
    const clientHeight = containerRight.clientHeight; // 可见区域高度
    const scrollHeight = containerRight.scrollHeight; // 内容总高度

    const threshold = scrollHeight - 200; // 提前 200px 触发加载

    if (scrollTop + clientHeight >= threshold) {
        currentPage++;
        getBlogs(currentPage);
    }
});

// 初始加载第一页
getBlogs(currentPage);

// 获取当前用户的用户名
function getCurrentUsername() {
    $.ajax({
        type: 'get',
        url: 'get_current_user', // 这里的URL是你的服务器端API，用于获取当前用户
        success: function(response) {
            if (response.username) {
                document.getElementById('username').innerHTML = response.username; // 替换用户名
            }
        },
        error: function(xhr, status, error) {
            console.error('获取用户名失败:', status, error);
        }
    });
}

// 调用获取用户名的函数
getCurrentUsername();