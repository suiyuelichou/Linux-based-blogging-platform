// 在文件顶部添加
function escapeHtml(unsafe) {
    if (!unsafe) return '';
    return unsafe
        .toString()
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

document.addEventListener("DOMContentLoaded", function () {
    const sidebarItems = document.querySelectorAll(".sidebar li");
    const tabContents = document.querySelectorAll(".tab-content");
    const pageTitle = document.getElementById("page-title");

    sidebarItems.forEach(item => {
        item.addEventListener("click", function () {
            sidebarItems.forEach(i => i.classList.remove("active"));
            this.classList.add("active");

            const tabId = this.getAttribute("data-tab");
            tabContents.forEach(content => {
                content.classList.remove("active");
                if (content.id === tabId) {
                    content.classList.add("active");
                }
            });

            pageTitle.textContent = this.textContent;
        });
    });

    document.querySelector(".theme-toggle").addEventListener("click", function () {
        const currentTheme = document.body.getAttribute("data-theme");
        const newTheme = currentTheme === "light" ? "dark" : "light";
        document.body.setAttribute("data-theme", newTheme);
        this.textContent = newTheme === "light" ? "🌙 切换模式" : "☀️ 切换模式";
    });

    fetchStats();
});

function fetchStats() {
    fetch("/admins/stats")
        .then(response => response.json())
        .then(data => {
            document.getElementById("totalPosts").textContent = data.totalPosts;
            document.getElementById("totalUsers").textContent = data.totalUsers;
            document.getElementById("totalComments").textContent = data.totalComments;
            document.getElementById("totalLikes").textContent = data.totalLikes;
            // document.getElementById("dailyVisits").textContent = data.dailyVisits;
        })
        .catch(error => console.error("获取统计数据失败:", error));
}









// 文章管理模块 - 添加命名空间避免冲突
const BlogPostManager = (function() {
    // 私有变量 - 添加blog前缀以明确区分
    let blogCurrentPage = 1;
    let blogTotalPages = 1;
    let blogPageSize = 10;
    let blogCurrentPostId = null; 
    let blogCategories = []; 

    // DOM元素选择器已添加前缀
    const elements = {
        postsTableBody: document.getElementById('blog-postsTableBody'),
        postModal: document.getElementById('blog-postModal'),
        deleteModal: document.getElementById('blog-deleteModal'),
        postForm: document.getElementById('blog-postForm'),
        modalTitle: document.getElementById('blog-modalTitle'),
        prevPageBtn: document.getElementById('blog-prevPage'),
        nextPageBtn: document.getElementById('blog-nextPage'),
        currentPageSpan: document.getElementById('blog-currentPage'),
        totalPagesSpan: document.getElementById('blog-totalPages'),
        categoryFilter: document.getElementById('blog-categoryFilter'),
        sortBySelect: document.getElementById('blog-sortBy'),
        postSearch: document.getElementById('blog-postSearch'),
        searchBtn: document.getElementById('blog-searchBtn'),
        cancelDeleteBtn: document.getElementById('blog-cancelDelete'),
        confirmDeleteBtn: document.getElementById('blog-confirmDelete'),
        closeBtn: document.querySelector('.blog-close')
    };

    // 初始化函数
    function init() {
        // 获取文章列表
        fetchPosts();
        
        // 获取分类列表
        fetchCategories();
        
        // 绑定事件监听器
        bindEvents();
    }
    
    // 将所有事件监听绑定抽取为单独的函数
    function bindEvents() {
        // 移除之前可能存在的事件监听以避免重复绑定
        if (elements.prevPageBtn) {
            elements.prevPageBtn.removeEventListener('click', handlePrevPage);
            elements.prevPageBtn.addEventListener('click', handlePrevPage);
        }
        
        if (elements.nextPageBtn) {
            elements.nextPageBtn.removeEventListener('click', handleNextPage);
            elements.nextPageBtn.addEventListener('click', handleNextPage);
        }
        
        if (elements.categoryFilter) {
            elements.categoryFilter.removeEventListener('change', applyFilters);
            elements.categoryFilter.addEventListener('change', applyFilters);
        }
        
        if (elements.sortBySelect) {
            elements.sortBySelect.removeEventListener('change', applyFilters);
            elements.sortBySelect.addEventListener('change', applyFilters);
        }
        
        if (elements.searchBtn) {
            elements.searchBtn.removeEventListener('click', applyFilters);
            elements.searchBtn.addEventListener('click', applyFilters);
        }
        
        if (elements.postSearch) {
            elements.postSearch.removeEventListener('keypress', handleSearchKeypress);
            elements.postSearch.addEventListener('keypress', handleSearchKeypress);
        }
        
        // 模态框关闭按钮
        if (elements.closeBtn) {
            elements.closeBtn.removeEventListener('click', closePostModal);
            elements.closeBtn.addEventListener('click', closePostModal);
        }
        
        if (elements.cancelDeleteBtn) {
            elements.cancelDeleteBtn.removeEventListener('click', closeDeleteModal);
            elements.cancelDeleteBtn.addEventListener('click', closeDeleteModal);
        }
        
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.removeEventListener('click', deletePost);
            elements.confirmDeleteBtn.addEventListener('click', deletePost);
        }
    }
    
    // 使用命名函数代替匿名函数，以便于移除事件监听
    function handlePrevPage() {
        navigatePage(blogCurrentPage - 1);
    }
    
    function handleNextPage() {
        navigatePage(blogCurrentPage + 1);
    }
    
    function handleSearchKeypress(e) {
        if (e.key === 'Enter') applyFilters();
    }

    // 获取文章列表
    function fetchPosts() {
        const categoryId = elements.categoryFilter.value;
        const sort = elements.sortBySelect.value;
        const search = elements.postSearch.value;
        
        // 构建查询参数
        const params = new URLSearchParams({
            page: blogCurrentPage,
            pageSize: blogPageSize,
            sort: sort
        });
        
        if (categoryId !== 'all') params.append('categoryId', categoryId);
        if (search) params.append('search', search);
        
        // 发送请求
        fetch(`/admins/posts?${params.toString()}`)
            .then(response => response.json())
            .then(data => {
                // 更新文章列表
                renderPostsTable(data.posts);
                
                // 更新分页信息
                blogTotalPages = data.totalPages;
                elements.currentPageSpan.textContent = blogCurrentPage;
                elements.totalPagesSpan.textContent = blogTotalPages;
                
                // 更新分页按钮状态
                elements.prevPageBtn.disabled = blogCurrentPage <= 1;
                elements.nextPageBtn.disabled = blogCurrentPage >= blogTotalPages;
            })
            .catch(error => {
                console.error('获取文章列表失败:', error);
                alert('获取文章列表失败，请稍后重试');
            });
    }

    // 渲染文章表格 - 将编辑按钮改为查看按钮
    function renderPostsTable(posts) {
        elements.postsTableBody.innerHTML = '';
        
        if (posts.length === 0) {
            elements.postsTableBody.innerHTML = `
                <tr>
                    <td colspan="6" style="text-align: center; padding: 20px;">
                        没有找到匹配的文章
                    </td>
                </tr>
            `;
            return;
        }
        
        posts.forEach(post => {
            const safeTitle = escapeHtml(post.title);
            const safeAuthor = escapeHtml(post.user || '未知作者');
            const safeCategory = escapeHtml(post.categoryName || '未分类');
            
            const tr = document.createElement('tr');
            
            // 格式化日期
            const createdDate = new Date(post.createdAt).toLocaleDateString('zh-CN');
            const updatedDate = new Date(post.updatedAt).toLocaleDateString('zh-CN');
            
            tr.innerHTML = `
                <td class="post-title">${safeTitle}</td>
                <td>${safeAuthor}</td>
                <td>${safeCategory}</td>
                <td>${createdDate}</td>
                <td>${updatedDate}</td>
                <td>${post.views || 0}</td>
                <td>
                    <button class="blog-view-btn post-action-btn" data-id="${post.id}" title="查看">👁️</button>
                    <button class="blog-delete-btn post-action-btn" data-id="${post.id}" title="删除">🗑️</button>
                </td>
            `;
            
            elements.postsTableBody.appendChild(tr);
        });
        
        // 为查看和删除按钮添加事件监听器
        document.querySelectorAll('.blog-view-btn').forEach(btn => {
            btn.addEventListener('click', () => openViewPostModal(btn.dataset.id));
        });
        
        document.querySelectorAll('.blog-delete-btn').forEach(btn => {
            btn.addEventListener('click', () => openDeleteConfirmation(btn.dataset.id));
        });
    }

    // 获取分类列表
    function fetchCategories() {
        fetch('/admins/blogcategories')
            .then(response => response.json())
            .then(data => {
                blogCategories = data;
                
                // 更新分类筛选下拉框
                if (elements.categoryFilter) {
                    elements.categoryFilter.innerHTML = '<option value="all">全部分类</option>';
                    
                    blogCategories.forEach(category => {
                        // 添加到筛选下拉框
                        const filterOption = document.createElement('option');
                        filterOption.value = category.id;
                        filterOption.textContent = category.name;
                        elements.categoryFilter.appendChild(filterOption);
                    });
                }
                
                // 更新文章表单中的分类下拉框
                const categorySelect = document.getElementById('blog-category');
                if (categorySelect) {
                    categorySelect.innerHTML = '';
                    
                    blogCategories.forEach(category => {
                        // 添加到表单下拉框
                        const formOption = document.createElement('option');
                        formOption.value = category.id;
                        formOption.textContent = category.name;
                        categorySelect.appendChild(formOption);
                    });
                }
            })
            .catch(error => {
                console.error('获取分类列表失败:', error);
            });
    }

    // 应用筛选器
    function applyFilters() {
        blogCurrentPage = 1; // 重置为第一页
        fetchPosts();
    }

    // 页面导航
    function navigatePage(page) {
        if (page < 1 || page > blogTotalPages) return;
        blogCurrentPage = page;
        fetchPosts();
    }

    // 打开查看文章模态框 (原编辑文章模态框)
    function openViewPostModal(postId) {
        elements.modalTitle.textContent = '查看文章';
        blogCurrentPostId = postId;
        
        // 获取文章详情
        fetch(`/admins/posts/${postId}`)
            .then(response => response.json())
            .then(post => {
                // 设置标题
                document.getElementById('blog-title').textContent = post.title;
                
                // 处理分类
                const categoryText = document.getElementById('blog-category-text');
                // 如果存在categoryId，尝试从blogCategories数组中查找对应的分类名称
                let categoryName = '未分类';
                if (post.categoryId) {
                    // 在下拉框中选中对应的分类
                    const categorySelect = document.getElementById('blog-category');
                    if (categorySelect) {
                        for (let i = 0; i < categorySelect.options.length; i++) {
                            if (categorySelect.options[i].value == post.categoryId) {
                                categorySelect.selectedIndex = i;
                                categoryName = categorySelect.options[i].textContent;
                                break;
                            }
                        }
                    }
                    
                    // 如果未能在下拉框中找到，则尝试在blogCategories数组中查找
                    if (categoryName === '未分类' && blogCategories.length > 0) {
                        const category = blogCategories.find(c => c.id == post.categoryId);
                        if (category) {
                            categoryName = category.name;
                        }
                    }
                }
                
                // 如果接口已经返回了categoryName，优先使用它
                if (post.categoryName) {
                    categoryName = post.categoryName;
                }
                
                // 设置分类文本
                categoryText.textContent = categoryName;
                
                // 处理作者
                // document.getElementById('blog-author').textContent = post.user || '未知作者';
                
                // 格式化发布日期
                const postDate = document.getElementById('blog-postDate');
                const createdDate = new Date(post.createdAt);
                postDate.textContent = createdDate.toLocaleDateString('zh-CN');
                
                // 处理缩略图
                const postImage = document.getElementById('blog-postImage');
                if (post.thumbnail && post.thumbnail.trim() !== '') {
                    postImage.src = post.thumbnail;
                    postImage.alt = post.title;
                    postImage.classList.add('has-image');
                } else {
                    postImage.classList.remove('has-image');
                }
                
                // 处理标签
                const tagsContainer = document.getElementById('blog-tagsContainer');
                tagsContainer.innerHTML = '';

                if (post.tags && post.tags.length > 0) {
                    post.tags.forEach(tag => {
                        // 不对标签文本进行HTML转义，因为textContent会自动安全处理内容
                        const tagElement = document.createElement('span');
                        tagElement.className = 'post-tag';
                        tagElement.textContent = tag; // 直接使用原始标签文本，不进行转义
                        tagsContainer.appendChild(tagElement);
                    });
                } else {
                    tagsContainer.innerHTML = '<span class="no-tags">暂无标签</span>';
                }
                
                // 处理内容
                const contentElement = document.getElementById('blog-content');
                
                // 检查内容格式，适配不同的内容格式
                if (post.content_format === 'markdown' && typeof post.content === 'string') {
                    // 如果是Markdown格式
                    if (window.marked) {
                        marked.setOptions({
                            sanitize: true // 开启marked的XSS防护
                        });
                        contentElement.innerHTML = marked.parse(post.content);
                    } else {
                        contentElement.innerHTML = `<pre>${post.content}</pre>`;
                    }
                } else if (post.content_html) {
                    // 如果有预渲染的HTML
                    contentElement.innerHTML = post.content_html;
                } else {
                    // 普通内容处理，保留换行和格式
                    const textWithBreaks = post.content.replace(/\n/g, '<br>');
                    contentElement.innerHTML = textWithBreaks;
                }
                
                // 处理可能的Quill编辑器代码块
                processContentFormatting(contentElement);
                
                // 显示模态框
                elements.postModal.style.display = 'block';
            })
            .catch(error => {
                console.error('获取文章详情失败:', error);
                showNotification('获取文章详情失败，请稍后重试', 'error');
            });
    }
    
    // 处理内容格式化
    function processContentFormatting(contentElement) {
        // 处理代码块
        const codeBlocks = contentElement.querySelectorAll('pre code');
        codeBlocks.forEach(codeBlock => {
            codeBlock.style.fontFamily = 'monospace';
            codeBlock.style.display = 'block';
            codeBlock.style.whiteSpace = 'pre';
            codeBlock.style.overflowX = 'auto';
        });
        
        // 处理内联代码
        const inlineCodes = contentElement.querySelectorAll('code:not(pre code)');
        inlineCodes.forEach(code => {
            code.style.backgroundColor = 'rgba(0, 0, 0, 0.05)';
            code.style.padding = '2px 4px';
            code.style.borderRadius = '3px';
            code.style.fontFamily = 'monospace';
            code.style.fontSize = '0.9em';
        });
        
        // 处理图片
        const images = contentElement.querySelectorAll('img');
        images.forEach(img => {
            img.style.maxWidth = '100%';
            img.style.height = 'auto';
            img.style.borderRadius = '5px';
            img.style.margin = '10px 0';
        });
        
        // 处理链接
        const links = contentElement.querySelectorAll('a');
        links.forEach(link => {
            link.style.color = 'var(--primary-color, #4e6ef2)';
            link.style.textDecoration = 'none';
            link.target = '_blank';
            link.rel = 'noopener noreferrer';
        });
    }
    
    // 显示通知
    function showNotification(message, type = 'info') {
        alert(message);  // 简单实现，可以替换为更美观的通知
    }

    // 关闭文章模态框
    function closePostModal() {
        elements.postModal.style.display = 'none';
    }

    // 打开删除确认对话框
    function openDeleteConfirmation(postId) {
        blogCurrentPostId = postId;
        elements.deleteModal.style.display = 'block';
    }

    // 关闭删除确认对话框
    function closeDeleteModal() {
        elements.deleteModal.style.display = 'none';
    }

    // 删除文章
    function deletePost() {
        if (!blogCurrentPostId) return;
        
        fetch(`/admins/posts/${blogCurrentPostId}`, {
            method: 'DELETE'
        })
            .then(response => {
                if (!response.ok) {
                    throw new Error('删除失败');
                }
                closeDeleteModal();
                fetchPosts(); // 刷新文章列表
                // alert('文章已成功删除');
            })
            .catch(error => {
                console.error('删除文章失败:', error);
                alert('删除文章失败，请稍后重试');
            });
    }

    // 公开接口
    return {
        init: init
    };
})();

// 当文章管理标签被点击时调用初始化函数
document.addEventListener('DOMContentLoaded', function() {
    const postTabTrigger = document.querySelector('[data-tab="posts"]');
    if (postTabTrigger) {
        postTabTrigger.addEventListener('click', function() {
            // 延迟初始化，确保DOM已完全加载
            setTimeout(BlogPostManager.init, 100);
        });
    }
});



































// 评论管理模块 - 使用立即执行函数创建命名空间避免冲突
const CommentManager = (function() {
    // 私有变量 - 添加comment前缀以明确区分
    let commentCurrentPage = 1;
    let commentTotalPages = 1;
    let commentPageSize = 10;
    let commentCurrentId = null; // 用于删除操作
    let commentIsInitialized = false; // 标记是否已初始化

    // DOM元素选择器添加前缀
    const elements = {
        commentsTableBody: document.getElementById('commentsTableBody'),
        deleteModal: document.getElementById('commentDeleteModal'),
        prevPageBtn: document.getElementById('commentPrevPage'),
        nextPageBtn: document.getElementById('commentNextPage'),
        currentPageSpan: document.getElementById('commentCurrentPage'),
        totalPagesSpan: document.getElementById('commentTotalPages'),
        sortBySelect: document.getElementById('commentSortBy'),
        commentSearch: document.getElementById('comment-search'),
        searchBtn: document.getElementById('comment-search-btn'),
        cancelDeleteBtn: document.getElementById('commentCancelDelete'),
        confirmDeleteBtn: document.getElementById('commentConfirmDelete')
    };

    // 初始化函数
    function init() {
        // 获取评论列表
        fetchComments();
        
        // 绑定事件监听器 - 只在首次初始化时添加
        if (!commentIsInitialized) {
            bindEventListeners();
            commentIsInitialized = true;
        }
    }

    // 绑定所有事件监听器
    function bindEventListeners() {
        // 使用命名函数以便于移除事件监听
        function handlePrevPage() {
            navigatePage(commentCurrentPage - 1);
        }
        
        function handleNextPage() {
            navigatePage(commentCurrentPage + 1);
        }
        
        function handleSortChange() {
            applyFilters();
        }
        
        function handleSearchClick() {
            applyFilters();
        }
        
        function handleSearchKeypress(e) {
            if (e.key === 'Enter') applyFilters();
        }
        
        // 为分页按钮添加事件监听
        if (elements.prevPageBtn) {
            elements.prevPageBtn.removeEventListener('click', handlePrevPage);
            elements.prevPageBtn.addEventListener('click', handlePrevPage);
        }
        
        if (elements.nextPageBtn) {
            elements.nextPageBtn.removeEventListener('click', handleNextPage);
            elements.nextPageBtn.addEventListener('click', handleNextPage);
        }
        
        // 为筛选器添加事件监听
        if (elements.sortBySelect) {
            elements.sortBySelect.removeEventListener('change', handleSortChange);
            elements.sortBySelect.addEventListener('change', handleSortChange);
        }
        
        if (elements.searchBtn) {
            elements.searchBtn.removeEventListener('click', handleSearchClick);
            elements.searchBtn.addEventListener('click', handleSearchClick);
        }
        
        if (elements.commentSearch) {
            elements.commentSearch.removeEventListener('keypress', handleSearchKeypress);
            elements.commentSearch.addEventListener('keypress', handleSearchKeypress);
        }
        
        // 为模态框按钮添加事件监听
        if (elements.cancelDeleteBtn) {
            elements.cancelDeleteBtn.removeEventListener('click', closeDeleteModal);
            elements.cancelDeleteBtn.addEventListener('click', closeDeleteModal);
        }
        
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.removeEventListener('click', deleteComment);
            elements.confirmDeleteBtn.addEventListener('click', deleteComment);
        }
        
        // 使用事件代理为表格添加事件监听
        if (elements.commentsTableBody) {
            // 移除之前的事件监听
            elements.commentsTableBody.removeEventListener('click', handleTableClick);
            elements.commentsTableBody.addEventListener('click', handleTableClick);
        }
    }
    
    // 表格点击事件处理
    function handleTableClick(e) {
        // 处理删除按钮点击
        if (e.target.classList.contains('delete-comment-btn')) {
            openDeleteConfirmation(e.target.dataset.id);
        }
        
        // 处理文章链接点击
        if (e.target.classList.contains('comment-article')) {
            window.open(`/blog_detail.html?id=${e.target.dataset.articleId}`, '_blank');
        }
    }

    // 获取评论列表
    function fetchComments() {
        // 获取当前的筛选值
        const sort = elements.sortBySelect ? elements.sortBySelect.value : '';
        const search = elements.commentSearch ? elements.commentSearch.value : '';
        
        // 构建查询参数
        const params = new URLSearchParams({
            page: commentCurrentPage,
            pageSize: commentPageSize
        });
        
        if (sort) params.append('sort', sort);
        if (search) params.append('search', search);
        
        // 显示加载中提示
        if (elements.commentsTableBody) {
            elements.commentsTableBody.innerHTML = '<tr><td colspan="5" class="loading-message">加载中...</td></tr>';
        }
        
        // 发送请求
        fetch(`/admins/comments?${params.toString()}`)
            .then(response => response.json())
            .then(data => {
                // 更新评论列表
                renderCommentsTable(data.comments);
                
                // 更新分页信息
                commentTotalPages = data.totalPages;
                if (elements.currentPageSpan) elements.currentPageSpan.textContent = commentCurrentPage;
                if (elements.totalPagesSpan) elements.totalPagesSpan.textContent = commentTotalPages;
                
                // 更新分页按钮状态
                if (elements.prevPageBtn) elements.prevPageBtn.disabled = commentCurrentPage <= 1;
                if (elements.nextPageBtn) elements.nextPageBtn.disabled = commentCurrentPage >= commentTotalPages;
            })
            .catch(error => {
                console.error('获取评论列表失败:', error);
                if (elements.commentsTableBody) {
                    elements.commentsTableBody.innerHTML = '<tr><td colspan="5" class="error-message">获取评论数据失败，请稍后重试</td></tr>';
                }
            });
    }

    // 渲染评论表格
    function renderCommentsTable(comments) {
        if (!elements.commentsTableBody) return;
        
        elements.commentsTableBody.innerHTML = '';
        
        if (comments.length === 0) {
            elements.commentsTableBody.innerHTML = `
                <tr>
                    <td colspan="5" class="no-data-message">没有找到匹配的评论</td>
                </tr>
            `;
            return;
        }
        
        comments.forEach(comment => {
            const safeAuthor = escapeHtml(comment.author || '匿名用户');
            const safeContent = escapeHtml(comment.content);
            const safeArticleTitle = escapeHtml(comment.articleTitle || '未知文章');
            
            const row = document.createElement('tr');
            
            // 格式化日期
            const commentDate = new Date(comment.createdAt).toLocaleDateString('zh-CN') + ' ' + 
                new Date(comment.createdAt).toLocaleTimeString('zh-CN', {hour: '2-digit', minute:'2-digit'});
            
            row.innerHTML = `
                <td>
                    <div class="comment-author">${safeAuthor}</div>
                </td>
                <td>
                    <div class="comment-preview">${safeContent}</div>
                </td>
                <td>
                    <div class="comment-article" data-article-id="${comment.articleId}" title="查看文章">${safeArticleTitle}</div>
                </td>
                <td>${commentDate}</td>
                <td>
                    <button class="comment-action-btn delete-comment-btn" data-id="${comment.id}" title="删除">🗑️</button>
                </td>
            `;
            
            elements.commentsTableBody.appendChild(row);
        });
    }

    // 应用筛选器
    function applyFilters() {
        commentCurrentPage = 1; // 重置为第一页
        fetchComments();
    }
    
    // 评论页面导航
    function navigatePage(page) {
        if (page < 1 || page > commentTotalPages) return;
        commentCurrentPage = page;
        fetchComments();
    }

    // 打开删除确认对话框
    function openDeleteConfirmation(commentId) {
        commentCurrentId = commentId;
        if (elements.deleteModal) elements.deleteModal.style.display = 'block';
    }

    // 关闭删除确认对话框
    function closeDeleteModal() {
        if (elements.deleteModal) elements.deleteModal.style.display = 'none';
    }

    // 删除评论
    function deleteComment() {
        if (!commentCurrentId) return;
        
        // 禁用删除按钮，防止重复点击
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.disabled = true;
            elements.confirmDeleteBtn.textContent = '删除中...';
        }
        
        fetch(`/admins/comments/${commentCurrentId}`, {
            method: 'DELETE'
        })
            .then(response => {
                if (!response.ok) {
                    throw new Error('删除失败');
                }
                closeDeleteModal();
                fetchComments(); // 刷新评论列表
                // alert('评论已成功删除');
            })
            .catch(error => {
                console.error('删除评论失败:', error);
                alert('删除评论失败，请稍后重试');
            })
            .finally(() => {
                // 恢复按钮状态
                if (elements.confirmDeleteBtn) {
                    elements.confirmDeleteBtn.disabled = false;
                    elements.confirmDeleteBtn.textContent = '删除';
                }
            });
    }

    // 公开接口
    return {
        init: init
    };
})();

// 当评论管理标签被点击时调用初始化函数
document.addEventListener('DOMContentLoaded', function() {
    const commentTabTrigger = document.querySelector('[data-tab="comments"]');
    if (commentTabTrigger) {
        commentTabTrigger.addEventListener('click', function() {
            // 每次点击时调用初始化函数（但内部会判断是否需要重新绑定事件）
            setTimeout(CommentManager.init, 100);
        });
    }
});


















// 用户管理模块 - 使用立即执行函数创建命名空间避免冲突
const UserManager = (function() {
    // 私有变量 - 添加user前缀以明确区分
    let userCurrentPage = 1;
    let userTotalPages = 1;
    let userPageSize = 10;
    let userCurrentId = null; // 用于编辑和删除操作
    let userIsInitialized = false; // 标记是否已初始化

    // DOM元素选择器添加前缀
    const elements = {
        usersTableBody: document.getElementById('usersTableBody'),
        userModal: document.getElementById('userModal'),
        deleteModal: document.getElementById('userDeleteModal'),
        userForm: document.getElementById('userForm'),
        modalTitle: document.getElementById('userModalTitle'),
        createUserBtn: document.getElementById('createUserBtn'),
        prevPageBtn: document.getElementById('userPrevPage'),
        nextPageBtn: document.getElementById('userNextPage'),
        currentPageSpan: document.getElementById('userCurrentPage'),
        totalPagesSpan: document.getElementById('userTotalPages'),
        statusFilter: document.getElementById('userStatusFilter'),
        sortBySelect: document.getElementById('userSortBy'),
        userSearch: document.getElementById('userSearch'),
        searchBtn: document.getElementById('userSearchBtn'),
        resetPasswordBtn: document.getElementById('resetPasswordBtn'),
        cancelDeleteBtn: document.getElementById('userCancelDelete'),
        confirmDeleteBtn: document.getElementById('userConfirmDelete')
    };

    // 初始化函数
    function init() {
        // 获取用户列表
        fetchUsers();
        
        // 绑定事件监听器 - 只在首次初始化时添加
        if (!userIsInitialized) {
            bindEventListeners();
            userIsInitialized = true;
        }
    }

    // 绑定所有事件监听器
    function bindEventListeners() {
        // 使用命名函数以便于移除事件监听
        function handlePrevPage() {
            navigateUserPage(userCurrentPage - 1);
        }
        
        function handleNextPage() {
            navigateUserPage(userCurrentPage + 1);
        }
        
        function handleSortChange() {
            applyUserFilters();
        }
        
        function handleStatusChange() {
            applyUserFilters();
        }
        
        function handleSearchClick() {
            applyUserFilters();
        }
        
        function handleSearchKeypress(e) {
            if (e.key === 'Enter') applyUserFilters();
        }
        
        function handleCreateUser() {
            openCreateUserModal();
        }
        
        function handleFormSubmit(e) {
            handleUserSubmit(e);
        }
        
        function handleResetPassword() {
            showPasswordFields();
        }
        
        // 为分页按钮添加事件监听
        if (elements.prevPageBtn) {
            elements.prevPageBtn.removeEventListener('click', handlePrevPage);
            elements.prevPageBtn.addEventListener('click', handlePrevPage);
        }
        
        if (elements.nextPageBtn) {
            elements.nextPageBtn.removeEventListener('click', handleNextPage);
            elements.nextPageBtn.addEventListener('click', handleNextPage);
        }
        
        // 为筛选器添加事件监听
        if (elements.statusFilter) {
            elements.statusFilter.removeEventListener('change', handleStatusChange);
            elements.statusFilter.addEventListener('change', handleStatusChange);
        }
        
        if (elements.sortBySelect) {
            elements.sortBySelect.removeEventListener('change', handleSortChange);
            elements.sortBySelect.addEventListener('change', handleSortChange);
        }
        
        if (elements.searchBtn) {
            elements.searchBtn.removeEventListener('click', handleSearchClick);
            elements.searchBtn.addEventListener('click', handleSearchClick);
        }
        
        if (elements.userSearch) {
            elements.userSearch.removeEventListener('keypress', handleSearchKeypress);
            elements.userSearch.addEventListener('keypress', handleSearchKeypress);
        }
        
        // 创建用户按钮
        if (elements.createUserBtn) {
            elements.createUserBtn.removeEventListener('click', handleCreateUser);
            elements.createUserBtn.addEventListener('click', handleCreateUser);
        }
        
        // 表单提交事件
        if (elements.userForm) {
            elements.userForm.removeEventListener('submit', handleFormSubmit);
            elements.userForm.addEventListener('submit', handleFormSubmit);
        }
        
        // 重置密码按钮
        if (elements.resetPasswordBtn) {
            elements.resetPasswordBtn.removeEventListener('click', handleResetPassword);
            elements.resetPasswordBtn.addEventListener('click', handleResetPassword);
        }
        
        // 模态框按钮
        const userModalCloseBtn = document.querySelector('#userModal .user-close');
        if (userModalCloseBtn) {
            userModalCloseBtn.removeEventListener('click', closeUserModal);
            userModalCloseBtn.addEventListener('click', closeUserModal);
        }
        
        if (elements.cancelDeleteBtn) {
            elements.cancelDeleteBtn.removeEventListener('click', closeDeleteModal);
            elements.cancelDeleteBtn.addEventListener('click', closeDeleteModal);
        }
        
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.removeEventListener('click', deleteUser);
            elements.confirmDeleteBtn.addEventListener('click', deleteUser);
        }
        
        // 使用事件代理为表格添加事件监听
        if (elements.usersTableBody) {
            // 移除之前的事件监听
            elements.usersTableBody.removeEventListener('click', handleTableClick);
            elements.usersTableBody.addEventListener('click', handleTableClick);
        }
    }
    
    // 表格点击事件处理
    function handleTableClick(e) {
        // 处理编辑按钮点击
        if (e.target.classList.contains('edit-user-btn')) {
            openEditUserModal(e.target.dataset.id);
        }
        
        // 处理删除按钮点击
        if (e.target.classList.contains('delete-user-btn')) {
            openDeleteConfirmation(e.target.dataset.id);
        }
    }

    // 获取用户列表
    function fetchUsers() {
        // 获取当前的筛选值
        const status = elements.statusFilter ? elements.statusFilter.value : 'all';
        const sort = elements.sortBySelect ? elements.sortBySelect.value : '';
        const search = elements.userSearch ? elements.userSearch.value : '';
        
        // 构建查询参数
        const params = new URLSearchParams({
            page: userCurrentPage,
            pageSize: userPageSize
        });
        
        if (sort) params.append('sort', sort);
        if (status !== 'all') params.append('status', status);
        if (search) params.append('search', search);
        
        // 显示加载中提示
        if (elements.usersTableBody) {
            elements.usersTableBody.innerHTML = '<tr><td colspan="8" class="loading-message">加载中...</td></tr>';
        }
        
        // 发送请求
        fetch(`/admins/users?${params.toString()}`)
            .then(response => response.json())
            .then(data => {
                // 更新用户列表
                renderUsersTable(data.users);
                
                // 更新分页信息
                userTotalPages = data.totalPages;
                if (elements.currentPageSpan) elements.currentPageSpan.textContent = userCurrentPage;
                if (elements.totalPagesSpan) elements.totalPagesSpan.textContent = userTotalPages;
                
                // 更新分页按钮状态
                if (elements.prevPageBtn) elements.prevPageBtn.disabled = userCurrentPage <= 1;
                if (elements.nextPageBtn) elements.nextPageBtn.disabled = userCurrentPage >= userTotalPages;
            })
            .catch(error => {
                console.error('获取用户列表失败:', error);
                if (elements.usersTableBody) {
                    elements.usersTableBody.innerHTML = '<tr><td colspan="8" class="error-message">获取用户数据失败，请稍后重试</td></tr>';
                }
            });
    }

    // 渲染用户表格
    function renderUsersTable(users) {
        if (!elements.usersTableBody) return;
        
        elements.usersTableBody.innerHTML = '';
        
        if (users.length === 0) {
            elements.usersTableBody.innerHTML = `
                <tr>
                    <td colspan="8" class="no-data-message">没有找到匹配的用户</td>
                </tr>
            `;
            return;
        }
        
        users.forEach(user => {
            const safeUsername = escapeHtml(user.username);
            const safeEmail = escapeHtml(user.email || '');
            const safeBio = escapeHtml(user.bio || '');
            
            const registeredDate = new Date(user.createdAt).toLocaleDateString('zh-CN');
            const lastLoginDate = user.lastLogin ? 
                new Date(user.lastLogin).toLocaleDateString('zh-CN') + ' ' + 
                new Date(user.lastLogin).toLocaleTimeString('zh-CN', {hour: '2-digit', minute:'2-digit'}) : 
                '从未登录';
            
            const statusClass = `status-${user.status}`;
            const statusText = {
                'active': '已激活',
                'blocked': '已封禁'
            }[user.status] || user.status;
            
            const firstLetter = safeUsername.charAt(0).toUpperCase();
            
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>
                    <div class="user-avatar">
                        ${user.avatar ? 
                            `<img src="${escapeHtml(user.avatar)}" alt="${safeUsername}">` : 
                            `<div class="avatar-placeholder">${firstLetter}</div>`
                        }
                    </div>
                </td>
                <td>${safeUsername}</td>
                <td>${safeEmail}</td>
                <td><span class="status-tag ${statusClass}">${statusText}</span></td>
                <td>${registeredDate}</td>
                <td>${lastLoginDate}</td>
                <td>${user.postsCount || 0}</td>
                <td>
                    <button class="post-action-btn edit-user-btn" data-id="${user.id}" title="编辑">✏️</button>
                    <button class="post-action-btn delete-user-btn" data-id="${user.id}" title="删除">🗑️</button>
                </td>
            `;
            
            elements.usersTableBody.appendChild(row);
        });
    }

    // 应用筛选器
    function applyUserFilters() {
        userCurrentPage = 1; // 重置为第一页
        fetchUsers();
    }
    
    // 用户页面导航
    function navigateUserPage(page) {
        if (page < 1 || page > userTotalPages) return;
        userCurrentPage = page;
        fetchUsers();
    }

    // 打开创建用户模态框
    function openCreateUserModal() {
        if (!elements.modalTitle || !elements.userForm || !elements.userModal) return;
        
        elements.modalTitle.textContent = '添加新用户';
        elements.userForm.reset();
        userCurrentId = null;
        
        // 显示密码字段
        const passwordFields = document.getElementById('passwordFields');
        if (passwordFields) passwordFields.style.display = 'flex';
        
        const passwordInput = document.getElementById('password');
        const confirmPasswordInput = document.getElementById('confirmPassword');
        if (passwordInput) passwordInput.required = true;
        if (confirmPasswordInput) confirmPasswordInput.required = true;
        
        // 确保所有字段都是可编辑的（清除只读属性）
        const usernameField = document.getElementById('username');
        const emailField = document.getElementById('email');
        const bioField = document.getElementById('bio');
        
        if (usernameField) {
            usernameField.readOnly = false;
            usernameField.classList.remove('readonly-field');
        }
        
        if (emailField) {
            emailField.readOnly = false;
            emailField.classList.remove('readonly-field');
        }
        
        if (bioField) {
            bioField.readOnly = false;
            bioField.classList.remove('readonly-field');
        }
        
        // 隐藏重置密码按钮
        elements.userForm.classList.add('creating-user');
        
        elements.userModal.style.display = 'block';
    }

    // 打开编辑用户模态框
    function openEditUserModal(userId) {
        if (!elements.modalTitle) return;
        
        elements.modalTitle.textContent = '编辑用户';
        userCurrentId = userId;
        
        // 重置表单状态
        resetUserFormState();
        
        // 隐藏密码字段（编辑时不需要）
        const passwordFields = document.getElementById('passwordFields');
        const passwordInput = document.getElementById('password');
        const confirmPasswordInput = document.getElementById('confirmPassword');
        
        if (passwordFields) passwordFields.style.display = 'none';
        if (passwordInput) {
            passwordInput.required = false;
            passwordInput.value = ''; // 清空密码输入框
        }
        if (confirmPasswordInput) {
            confirmPasswordInput.required = false;
            confirmPasswordInput.value = ''; // 清空确认密码输入框
        }
        
        // 显示重置密码按钮
        if (elements.userForm) {
            elements.userForm.classList.remove('creating-user');
            elements.userForm.classList.add('editing-user');
        }
        
        // 显示加载指示器
        if (elements.userModal) {
            elements.userModal.style.display = 'block';
            // 可以添加加载指示器
            const formContent = elements.userModal.querySelector('form');
            if (formContent) {
                formContent.innerHTML = '<div class="loading-indicator">加载中...</div>';
            }
        }
        
        // 获取用户详情
        fetch(`/admins/users/${userId}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('获取用户数据失败');
                }
                return response.json();
            })
            .then(user => {
                // 存储原始用户数据，用于比较修改
                window.originalUserData = user;

                // 恢复表单内容
                if (elements.userForm) {
                    elements.userForm.innerHTML = getUserFormHtml();
                }

                // 填充表单
                const usernameField = document.getElementById('username');
                const emailField = document.getElementById('email');
                const statusField = document.getElementById('userStatus');
                const bioField = document.getElementById('bio');
                const resetPasswordBtn = document.getElementById('resetPasswordBtn');
                
                // 填充数据并设置只读属性
                if (usernameField) {
                    usernameField.value = user.username || '';
                    usernameField.readOnly = true; // 设置为只读
                    usernameField.classList.add('readonly-field');
                }
                
                if (emailField) {
                    emailField.value = user.email || '';
                    emailField.readOnly = true; // 设置为只读
                    emailField.classList.add('readonly-field');
                }
                
                if (statusField) {
                    // 确保设置正确的状态 - 状态是可编辑的
                    statusField.value = user.status || 'active'; 
                }
                
                if (bioField) {
                    bioField.value = user.bio || '';
                    bioField.readOnly = true; // 设置为只读
                    bioField.classList.add('readonly-field');
                }
                
                // 重新绑定事件
                if (resetPasswordBtn) {
                    resetPasswordBtn.addEventListener('click', showPasswordFields);
                }
                
                // 重新绑定表单提交事件
                const userForm = document.getElementById('userForm');
                if (userForm) {
                    userForm.addEventListener('submit', handleUserSubmit);
                }
            })
            .catch(error => {
                console.error('获取用户详情失败:', error);
                if (elements.userModal) {
                    const modalContent = elements.userModal.querySelector('.user-modal-content');
                    if (modalContent) {
                        modalContent.innerHTML = `
                            <span class="user-close">&times;</span>
                            <div class="error-message">
                                <h3>获取用户数据失败</h3>
                                <p>请稍后重试或联系管理员</p>
                                <button class="secondary-btn" onclick="closeUserModal()">关闭</button>
                            </div>
                        `;
                        
                        // 重新绑定关闭按钮事件
                        const closeBtn = modalContent.querySelector('.user-close');
                        if (closeBtn) {
                            closeBtn.addEventListener('click', closeUserModal);
                        }
                    }
                }
            });
    }
    
    // 重置用户表单状态
    function resetUserFormState() {
        // 重置表单验证状态和样式
        const form = elements.userForm;
        if (form) {
            form.classList.remove('creating-user', 'editing-user');
            
            // 重置可能存在的错误消息
            const errorMessages = form.querySelectorAll('.error-message');
            errorMessages.forEach(el => el.remove());
            
            // 重置输入字段样式
            const inputs = form.querySelectorAll('input, textarea, select');
            inputs.forEach(input => {
                input.classList.remove('readonly-field');
                input.readOnly = false;
            });
        }
    }
    
    // 获取用户表单HTML
    function getUserFormHtml() {
        return `
            <input type="hidden" id="userId" value="${userCurrentId || ''}">
            <div class="form-row">
                <div class="form-group">
                    <label for="username">用户名</label>
                    <input type="text" id="username" name="username" required pattern="[a-zA-Z0-9]{6,20}" title="用户名只能包含字母、数字，长度6-20位">
                    <small class="field-help">只能包含字母、数字,长度6-20位</small>
                </div>
                <div class="form-group">
                    <label for="email">邮箱</label>
                    <input type="email" id="email" name="email">
                </div>
            </div>
            <div class="form-row" id="passwordFields" style="display: none;">
                <div class="form-group">
                    <label for="password">密码</label>
                    <input type="password" id="password" name="password" pattern="^(?=.*[A-Za-z])(?=.*[\\d\\W]).{8,16}$" title="密码8-16位，包含字母和（数字或符号）">
                    <small class="field-help">密码8-16位，包含字母和（数字或符号）</small>
                </div>
                <div class="form-group">
                    <label for="confirmPassword">确认密码</label>
                    <input type="password" id="confirmPassword" name="confirmPassword">
                    <small class="field-help">请再次输入相同的密码</small>
                </div>
            </div>
            <div class="form-row">
                <div class="form-group">
                    <label for="userStatus">状态</label>
                    <select id="userStatus" name="status">
                        <option value="active">已激活</option>
                        <option value="blocked">已封禁</option>
                    </select>
                </div>
            </div>
            <div class="form-group">
                <label for="bio">个人简介</label>
                <textarea id="bio" name="bio" rows="3"></textarea>
            </div>
            <div class="form-actions">
                <button type="button" id="resetPasswordBtn" class="secondary-btn">重置密码</button>
                <button type="submit" class="primary-btn">保存</button>
            </div>
        `;
    }

    // 显示密码字段（用于密码重置）
    function showPasswordFields() {
        const passwordFields = document.getElementById('passwordFields');
        if (!passwordFields || !elements.resetPasswordBtn) return;
        
        if (passwordFields.style.display === 'none') {
            passwordFields.style.display = 'flex';
            document.getElementById('password').value = '';
            document.getElementById('confirmPassword').value = '';
            elements.resetPasswordBtn.textContent = '取消重置密码';
        } else {
            passwordFields.style.display = 'none';
            elements.resetPasswordBtn.textContent = '重置密码';
        }
    }

    // 关闭用户模态框
    function closeUserModal() {
        if (elements.userModal) elements.userModal.style.display = 'none';
    }

    // 处理用户表单提交
    function handleUserSubmit(e) {
        e.preventDefault();
        
        // 检查密码匹配（如果显示了密码字段）
        const passwordField = document.getElementById('password');
        const confirmPasswordField = document.getElementById('confirmPassword');
        const usernameField = document.getElementById('username');
        const emailField = document.getElementById('email');
        const statusField = document.getElementById('userStatus');
        const bioField = document.getElementById('bio');
        
        // 验证用户名格式（仅在创建模式下）
        if (!userCurrentId && usernameField) {
            const usernamePattern = /^[a-zA-Z0-9]{6,20}$/;
            if (!usernamePattern.test(usernameField.value)) {
                alert('用户名只能包含字母、数字，长度6-20位');
                return;
            }
        }
        
        // 验证密码（如果有密码输入）
        if (passwordField && passwordField.value) {
            // 密码格式：8-16位，包含字母和（数字或符号）
            const passwordPattern = /^(?=.*[A-Za-z])(?=.*[\d\W]).{8,16}$/;
            if (!passwordPattern.test(passwordField.value)) {
                alert('密码必须为8-16位，包含字母和（数字或符号）');
                return;
            }
            
            // 检查密码匹配
            if (confirmPasswordField && passwordField.value !== confirmPasswordField.value) {
                alert('两次输入的密码不匹配');
                return;
            }
        }
        
        if (!usernameField || !emailField || !statusField) {
            alert('表单数据不完整');
            return;
        }
        
        // 初始化表单数据对象
        const formData = {};
        
        // 原始用户数据（假设已在编辑时存储）
        const originalUserData = window.originalUserData || {};
        
        // 编辑模式 vs 创建模式的不同处理
        if (userCurrentId) {
            // 编辑模式 - 只允许修改状态和密码
            
            // 检查状态是否被修改
            if (statusField.value !== originalUserData.status) {
                formData.status = statusField.value;
            }
            
            // 仅在密码字段有值时添加密码
            if (passwordField && passwordField.value) {
                formData.password = passwordField.value;
            }
            
            // 如果没有任何修改，则提示并返回
            if (Object.keys(formData).length === 0) {
                alert('没有任何修改');
                return;
            }
        } else {
            // 创建模式 - 允许设置所有字段
            formData.username = usernameField.value;
            formData.email = emailField.value;
            formData.status = statusField.value;
            if (bioField) formData.bio = bioField.value;
            if (passwordField && passwordField.value) formData.password = passwordField.value;
        }
        
        // 创建或更新用户
        const url = userCurrentId ? `/admins/users/${userCurrentId}` : '/admins/users';
        const method = userCurrentId ? 'PATCH' : 'POST';
        
        fetch(url, {
            method: method,
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(formData)
        })
            .then(response => {
                if (!response.ok) {
                    throw new Error('操作失败');
                }
                return response.json();
            })
            .then(() => {
                closeUserModal();
                fetchUsers(); // 刷新用户列表
                // alert(userCurrentId ? '用户更新成功' : '用户创建成功');
            })
            .catch(error => {
                console.error('保存用户失败:', error);
                alert('保存用户失败，请稍后重试');
            });
    }

    // 打开删除确认对话框
    function openDeleteConfirmation(userId) {
        userCurrentId = userId;
        if (elements.deleteModal) elements.deleteModal.style.display = 'block';
    }

    // 关闭删除确认对话框
    function closeDeleteModal() {
        if (elements.deleteModal) elements.deleteModal.style.display = 'none';
    }

    // 删除用户
    function deleteUser() {
        if (!userCurrentId) return;
        
        // 禁用删除按钮，防止重复点击
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.disabled = true;
            elements.confirmDeleteBtn.textContent = '删除中...';
        }
        
        fetch(`/admins/users/${userCurrentId}`, {
            method: 'DELETE'
        })
            .then(response => {
                if (!response.ok) {
                    throw new Error('删除失败');
                }
                closeDeleteModal();
                fetchUsers(); // 刷新用户列表
                // alert('用户已成功删除');
            })
            .catch(error => {
                console.error('删除用户失败:', error);
                alert('删除用户失败，请稍后重试');
            })
            .finally(() => {
                // 恢复按钮状态
                if (elements.confirmDeleteBtn) {
                    elements.confirmDeleteBtn.disabled = false;
                    elements.confirmDeleteBtn.textContent = '删除';
                }
            });
    }

    // 公开接口
    return {
        init: init
    };
})();

// 当用户管理标签被点击时调用初始化函数
document.addEventListener('DOMContentLoaded', function() {
    const userTabTrigger = document.querySelector('[data-tab="users"]');
    if (userTabTrigger) {
        userTabTrigger.addEventListener('click', function() {
            // 每次点击时调用初始化函数（但内部会判断是否需要重新绑定事件）
            setTimeout(UserManager.init, 100);
        });
    }
});




























// 分类管理相关的JavaScript代码
// 分类管理模块 - 使用立即执行函数创建命名空间避免冲突
const CategoryManager = (function() {
    // 私有变量 - 添加category前缀以明确区分
let categoryCurrentPage = 1;
let categoryTotalPages = 1;
let categoryPageSize = 10;
    let categoryCurrentId = null; // 用于编辑和删除操作
    let categoryIsInitialized = false; // 标记是否已初始化

    // DOM元素选择器添加前缀
    const elements = {
        categoriesTableBody: document.getElementById('categoriesTableBody'),
        categoryModal: document.getElementById('categoryModal'),
        deleteModal: document.getElementById('categorydeleteModal'),
        categoryForm: document.getElementById('categoryForm'),
        modalTitle: document.getElementById('categorymodalTitle'),
        createCategoryBtn: document.getElementById('createCategoryBtn'),
        prevPageBtn: document.getElementById('categoryprevPage'),
        nextPageBtn: document.getElementById('categorynextPage'),
        currentPageSpan: document.getElementById('categorycurrentPage'),
        totalPagesSpan: document.getElementById('categorytotalPages'),
        sortBySelect: document.getElementById('categorySortBy'),
        categorySearch: document.getElementById('category2Search'),
        searchBtn: document.getElementById('search2CatBtn'),
        cancelDeleteBtn: document.getElementById('categorycancelDelete'),
        confirmDeleteBtn: document.getElementById('categoryconfirmDelete')
    };

    // 初始化函数
    function init() {
    // 获取分类列表
    fetchCategories();
    
        // 绑定事件监听器 - 只在首次初始化时添加
        if (!categoryIsInitialized) {
            bindEventListeners();
            categoryIsInitialized = true;
        }
    }

    // 绑定所有事件监听器
    function bindEventListeners() {
        // 使用命名函数以便于移除事件监听
        function handlePrevPage() {
            navigateCategoryPage(categoryCurrentPage - 1);
        }
        
        function handleNextPage() {
            navigateCategoryPage(categoryCurrentPage + 1);
        }
        
        function handleSortChange() {
            applyCategoryFilters();
        }
        
        function handleSearchClick() {
            applyCategoryFilters();
        }
        
        function handleSearchKeypress(e) {
            if (e.key === 'Enter') applyCategoryFilters();
        }
        
        function handleCreateCategory() {
            openCreateCategoryModal();
        }
        
        function handleFormSubmit(e) {
            handleCategorySubmit(e);
        }
        
        // 为分页按钮添加事件监听
        if (elements.prevPageBtn) {
            elements.prevPageBtn.removeEventListener('click', handlePrevPage);
            elements.prevPageBtn.addEventListener('click', handlePrevPage);
        }
        
        if (elements.nextPageBtn) {
            elements.nextPageBtn.removeEventListener('click', handleNextPage);
            elements.nextPageBtn.addEventListener('click', handleNextPage);
        }
        
        // 为筛选器添加事件监听
        if (elements.sortBySelect) {
            elements.sortBySelect.removeEventListener('change', handleSortChange);
            elements.sortBySelect.addEventListener('change', handleSortChange);
        }
        
        if (elements.searchBtn) {
            elements.searchBtn.removeEventListener('click', handleSearchClick);
            elements.searchBtn.addEventListener('click', handleSearchClick);
        }
        
        if (elements.categorySearch) {
            elements.categorySearch.removeEventListener('keypress', handleSearchKeypress);
            elements.categorySearch.addEventListener('keypress', handleSearchKeypress);
        }
        
        // 创建分类按钮
        if (elements.createCategoryBtn) {
            elements.createCategoryBtn.removeEventListener('click', handleCreateCategory);
            elements.createCategoryBtn.addEventListener('click', handleCreateCategory);
        }
        
        // 表单提交事件
        if (elements.categoryForm) {
            elements.categoryForm.removeEventListener('submit', handleFormSubmit);
            elements.categoryForm.addEventListener('submit', handleFormSubmit);
        }
        
        // 模态框按钮
        const categoryModalCloseBtn = document.querySelector('#categoryModal .cat-close');
        if (categoryModalCloseBtn) {
            categoryModalCloseBtn.removeEventListener('click', closeCategoryModal);
            categoryModalCloseBtn.addEventListener('click', closeCategoryModal);
        }
        if (elements.cancelDeleteBtn) {
            elements.cancelDeleteBtn.removeEventListener('click', closeDeleteModal);
            elements.cancelDeleteBtn.addEventListener('click', closeDeleteModal);
        }
        
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.removeEventListener('click', deleteCategory);
            elements.confirmDeleteBtn.addEventListener('click', deleteCategory);
        }
        
        // 使用事件代理为表格添加事件监听
        if (elements.categoriesTableBody) {
            // 移除之前的事件监听
            elements.categoriesTableBody.removeEventListener('click', handleTableClick);
            elements.categoriesTableBody.addEventListener('click', handleTableClick);
        }
    }
    
    // 表格点击事件处理
    function handleTableClick(e) {
        // 处理编辑按钮点击
        if (e.target.classList.contains('edit-category-btn')) {
            openEditCategoryModal(e.target.dataset.id);
        }
        
        // 处理删除按钮点击
        if (e.target.classList.contains('delete-category-btn')) {
            openDeleteConfirmation(e.target.dataset.id);
        }
    }

    // 获取分类列表
    function fetchCategories() {
        // 获取当前的筛选值
        const sort = elements.sortBySelect ? elements.sortBySelect.value : '';
        const search = elements.categorySearch ? elements.categorySearch.value : '';
        
        // 构建查询参数
        const params = new URLSearchParams({
            page: categoryCurrentPage,
            pageSize: categoryPageSize
        });
        
        if (sort) params.append('sort', sort);
        if (search) params.append('search', search);
        
        // 显示加载中提示
        if (elements.categoriesTableBody) {
            elements.categoriesTableBody.innerHTML = '<tr><td colspan="6" class="loading-message">加载中...</td></tr>';
        }
        
        // 发送请求
        fetch(`/admins/categories?${params.toString()}`)
            .then(response => response.json())
            .then(data => {
                // 更新分类列表
                renderCategoriesTable(data.categories);
                
                // 更新分页信息
                categoryTotalPages = data.totalPages;
                if (elements.currentPageSpan) elements.currentPageSpan.textContent = categoryCurrentPage;
                if (elements.totalPagesSpan) elements.totalPagesSpan.textContent = categoryTotalPages;
                
                // 更新分页按钮状态
                if (elements.prevPageBtn) elements.prevPageBtn.disabled = categoryCurrentPage <= 1;
                if (elements.nextPageBtn) elements.nextPageBtn.disabled = categoryCurrentPage >= categoryTotalPages;
            })
            .catch(error => {
                console.error('获取分类列表失败:', error);
                if (elements.categoriesTableBody) {
                    elements.categoriesTableBody.innerHTML = '<tr><td colspan="6" class="error-message">获取分类数据失败，请稍后重试</td></tr>';
                }
            });
    }

    // 渲染分类表格
    function renderCategoriesTable(categories) {
        if (!elements.categoriesTableBody) return;
        
        elements.categoriesTableBody.innerHTML = '';
        
        if (categories.length === 0) {
            elements.categoriesTableBody.innerHTML = `
                <tr>
                    <td colspan="6" class="no-data-message">没有找到匹配的分类</td>
                </tr>
            `;
            return;
        }
        
        categories.forEach(category => {
            const safeName = escapeHtml(category.name);
            const safeDescription = escapeHtml(category.description || '无描述');
            
            const createdDate = new Date(category.createdAt).toLocaleDateString('zh-CN');
            const firstLetter = safeName.charAt(0).toUpperCase();
            
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>
                    <div class="category-icon">
                        ${category.icon ? 
                            `<span>${escapeHtml(category.icon)}</span>` : 
                            `<span>${firstLetter}</span>`
                        }
                    </div>
                </td>
                <td>${safeName}</td>
                <td><div class="category-description" title="${safeDescription}">${safeDescription}</div></td>
                <td>${createdDate}</td>
                <td>${category.articlesCount || 0}</td>
                <td>
                    <button class="post-action-btn edit-category-btn" data-id="${category.id}" title="编辑">✏️</button>
                    <button class="post-action-btn delete-category-btn" data-id="${category.id}" title="删除">🗑️</button>
                </td>
            `;
            
            elements.categoriesTableBody.appendChild(row);
        });
    }

    // 应用筛选器
    function applyCategoryFilters() {
        categoryCurrentPage = 1; // 重置为第一页
        fetchCategories();
    }
    
    // 分类页面导航
    function navigateCategoryPage(page) {
        if (page < 1 || page > categoryTotalPages) return;
        categoryCurrentPage = page;
        fetchCategories();
    }

    // 打开创建分类模态框
    function openCreateCategoryModal() {
        if (!elements.modalTitle || !elements.categoryForm || !elements.categoryModal) return;
        
        elements.modalTitle.textContent = '添加新分类';
        elements.categoryForm.reset();
        categoryCurrentId = null;
        
        elements.categoryModal.style.display = 'block';
    }

    // 打开编辑分类模态框
    function openEditCategoryModal(categoryId) {
        if (!elements.modalTitle) return;
        
        elements.modalTitle.textContent = '编辑分类';
        categoryCurrentId = categoryId;
        
        // 获取分类详情
        fetch(`/admins/categories/${categoryId}`)
            .then(response => response.json())
            .then(category => {
                // 存储原始分类数据，用于比较修改
                window.originalCategoryData = category;

                // 填充表单
                const nameField = document.getElementById('categoryName');
                const descriptionField = document.getElementById('categoryDescription');
                
                if (nameField) nameField.value = category.name;
                if (descriptionField) descriptionField.value = category.description || '';
                
                // 显示模态框
                if (elements.categoryModal) elements.categoryModal.style.display = 'block';
            })
            .catch(error => {
                console.error('获取分类详情失败:', error);
                alert('获取分类详情失败，请稍后重试');
            });
    }

    // 关闭分类模态框
    function closeCategoryModal() {
        if (elements.categoryModal) elements.categoryModal.style.display = 'none';
    }

    // 处理分类表单提交
    function handleCategorySubmit(e) {
        e.preventDefault();
        
        // 获取表单数据
        const nameField = document.getElementById('categoryName');
        const descriptionField = document.getElementById('categoryDescription');
        
        if (!nameField) {
            alert('表单数据不完整');
            return;
        }
        
        // 只发送被修改的字段（编辑模式）
        const formData = {};
        
        // 原始分类数据（假设已在编辑时存储）
        const originalCategoryData = window.originalCategoryData || {};
        
        // 如果是新建分类，发送所有字段
        if (!categoryCurrentId) {
            formData.name = nameField.value;
            formData.description = descriptionField ? descriptionField.value : '';
        } else {
            // 编辑模式：检查每个字段是否被修改
            if (nameField.value !== originalCategoryData.name) {
                formData.name = nameField.value;
            }
            
            if (descriptionField && descriptionField.value !== (originalCategoryData.description || '')) {
                formData.description = descriptionField.value;
            }
        }
        
        // 编辑模式：如果没有任何修改，则不发送请求
        if (categoryCurrentId && Object.keys(formData).length === 0) {
            alert('没有任何修改');
            return;
        }
        
        // 创建或更新分类
        const url = categoryCurrentId ? `/admins/categories/${categoryCurrentId}` : '/admins/categories';
        const method = categoryCurrentId ? 'PATCH' : 'POST';
        
        fetch(url, {
            method: method,
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(formData)
        })
            .then(response => {
                if (!response.ok) {
                    // 尝试解析错误消息
                    return response.json().then(err => {
                        throw new Error(err.message || '操作失败');
                    });
                }
                return response.json();
            })
            .then(() => {
                closeCategoryModal();
                fetchCategories(); // 刷新分类列表
                // alert(categoryCurrentId ? '分类更新成功' : '分类创建成功');
            })
            .catch(error => {
                console.error('保存分类失败:', error);
                alert(`保存分类失败: ${error.message || '请稍后重试'}`);
            });
    }

    // 打开删除确认对话框
    function openDeleteConfirmation(categoryId) {
        categoryCurrentId = categoryId;
        if (elements.deleteModal) elements.deleteModal.style.display = 'block';
    }

    // 关闭删除确认对话框
    function closeDeleteModal() {
        if (elements.deleteModal) elements.deleteModal.style.display = 'none';
    }

    // 删除分类
    function deleteCategory() {
        if (!categoryCurrentId) return;
        
        // 禁用删除按钮，防止重复点击
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.disabled = true;
            elements.confirmDeleteBtn.textContent = '删除中...';
        }
        
        fetch(`/admins/categories/${categoryCurrentId}`, {
            method: 'DELETE'
        })
            .then(response => {
                if (!response.ok) {
                    // 尝试解析错误消息
                    return response.json().then(err => {
                        throw new Error(err.message || '删除失败');
                    });
                }
                return response.json();
            })
            .then(() => {
                closeDeleteModal();
                fetchCategories(); // 刷新分类列表
                // alert('分类已成功删除');
            })
            .catch(error => {
                console.error('删除分类失败:', error);
                alert(`删除分类失败: ${error.message || '请稍后重试'}`);
            })
            .finally(() => {
                // 恢复按钮状态
                if (elements.confirmDeleteBtn) {
                    elements.confirmDeleteBtn.disabled = false;
                    elements.confirmDeleteBtn.textContent = '删除';
                }
            });
    }

    // 公开接口
    return {
        init: init
    };
})();

// 当分类管理标签被点击时调用初始化函数
document.addEventListener('DOMContentLoaded', function() {
    const categoryTabTrigger = document.querySelector('[data-tab="categories"]');
    if (categoryTabTrigger) {
        categoryTabTrigger.addEventListener('click', function() {
            // 每次点击时调用初始化函数（但内部会判断是否需要重新绑定事件）
            setTimeout(CategoryManager.init, 100);
        });
    }
});
















// 标签管理模块
// 标签管理模块
const TagManager = (function() {
    // 私有变量
    let tagCurrentPage = 1;
    let tagTotalPages = 1;
    let tagPageSize = 10;
    let tagCurrentId = null; // 用于删除操作
    let tagIsInitialized = false; // 标记是否已初始化
    
    // DOM元素选择器 - 使用惰性加载
    const elements = {
        get tagsTableBody() { return document.getElementById('tagsTableBody'); },
        get tagModal() { return document.getElementById('tagModal'); },
        get deleteModal() { return document.getElementById('tagDeleteModal'); },
        get tagForm() { return document.getElementById('tagForm'); },
        get modalTitle() { return document.getElementById('tagModalTitle'); },
        get createTagBtn() { return document.getElementById('createTagBtn'); },
        get prevPageBtn() { return document.getElementById('tagPrevPage'); },
        get nextPageBtn() { return document.getElementById('tagNextPage'); },
        get currentPageSpan() { return document.getElementById('tagCurrentPage'); },
        get totalPagesSpan() { return document.getElementById('tagTotalPages'); },
        get sortBySelect() { return document.getElementById('tagSortBy'); },
        get tagSearch() { return document.getElementById('tagSearch'); },
        get searchBtn() { return document.getElementById('tagSearchBtn'); },
        get cancelDeleteBtn() { return document.getElementById('tagCancelDelete'); },
        get confirmDeleteBtn() { return document.getElementById('tagConfirmDelete'); }
    };

    // 初始化函数
    function init() {
        // 获取标签列表
        fetchTags();
        
        // 绑定事件监听器 - 只在首次初始化时添加
        if (!tagIsInitialized) {
            bindEventListeners();
            tagIsInitialized = true;
        }
    }

    // 绑定所有事件监听器
    function bindEventListeners() {
        // 分页按钮事件
        elements.prevPageBtn?.addEventListener('click', () => navigateTagPage(tagCurrentPage - 1));
        elements.nextPageBtn?.addEventListener('click', () => navigateTagPage(tagCurrentPage + 1));
        
        // 筛选器事件
        elements.sortBySelect?.addEventListener('change', applyTagFilters);
        elements.searchBtn?.addEventListener('click', applyTagFilters);
        elements.tagSearch?.addEventListener('keypress', e => {
            if (e.key === 'Enter') applyTagFilters();
        });
        
        // 创建标签按钮
        elements.createTagBtn?.addEventListener('click', openCreateTagModal);
        
        // 表单提交事件
        elements.tagForm?.addEventListener('submit', handleTagSubmit);
        
        // 模态框关闭按钮
        document.querySelector('#tagModal .tag-close')?.addEventListener('click', closeTagModal);
        
        // 删除确认模态框按钮
        elements.cancelDeleteBtn?.addEventListener('click', closeDeleteModal);
        elements.confirmDeleteBtn?.addEventListener('click', deleteTag);
        
        // 使用事件代理为表格添加事件监听
        elements.tagsTableBody?.addEventListener('click', handleTableClick);
    }
    
    // 表格点击事件处理
    function handleTableClick(e) {
        // 只处理删除按钮点击
        if (e.target.classList.contains('delete-tag-btn')) {
            openDeleteConfirmation(e.target.dataset.id);
        }
    }

    // 获取标签列表
    function fetchTags() {
        // 获取当前的筛选值
        const sort = elements.sortBySelect?.value || '';
        const search = elements.tagSearch?.value || '';
        
        // 构建查询参数
        const params = new URLSearchParams({
            page: tagCurrentPage,
            pageSize: tagPageSize
        });
        
        if (sort) params.append('sort', sort);
        if (search) params.append('search', search);
        
        // 显示加载中提示
        if (elements.tagsTableBody) {
            elements.tagsTableBody.innerHTML = '<tr><td colspan="4" class="loading-message">加载中...</td></tr>';
        }
        
        // 发送请求
        fetch(`/admins/tags?${params.toString()}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP错误 ${response.status}`);
                }
                return response.json();
            })
            .then(data => {
                // 更新标签列表
                renderTagsTable(data.tags);
                
                // 更新分页信息
                tagTotalPages = data.totalPages || 1;
                if (elements.currentPageSpan) elements.currentPageSpan.textContent = tagCurrentPage;
                if (elements.totalPagesSpan) elements.totalPagesSpan.textContent = tagTotalPages;
                
                // 更新分页按钮状态
                if (elements.prevPageBtn) elements.prevPageBtn.disabled = tagCurrentPage <= 1;
                if (elements.nextPageBtn) elements.nextPageBtn.disabled = tagCurrentPage >= tagTotalPages;
            })
            .catch(error => {
                console.error('获取标签列表失败:', error);
                if (elements.tagsTableBody) {
                    elements.tagsTableBody.innerHTML = '<tr><td colspan="4" class="error-message">获取标签数据失败，请稍后重试</td></tr>';
                }
            });
    }

    // 渲染标签表格
    function renderTagsTable(tags) {
        if (!elements.tagsTableBody) return;
        
        elements.tagsTableBody.innerHTML = '';
        
        if (!tags || tags.length === 0) {
            elements.tagsTableBody.innerHTML = `
                <tr>
                    <td colspan="4" class="no-data-message">没有找到匹配的标签</td>
                </tr>
            `;
            return;
        }
        
        tags.forEach(tag => {
            const safeName = escapeHtml(tag.name);
            const safeDescription = escapeHtml(tag.description || '无描述');
            
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${safeName}</td>
                <td><div class="tag-description" title="${safeDescription}">${safeDescription}</div></td>
                <td>${new Date(tag.createdAt).toLocaleDateString('zh-CN')}</td>
                <td>${tag.blogCount || tag.articlesCount || tag.article_count || 0}</td>
                <td>
                    <button class="post-action-btn delete-tag-btn" data-id="${tag.id}" title="删除">🗑️</button>
                </td>
            `;
            
            elements.tagsTableBody.appendChild(row);
        });
    }

    // 应用筛选器
    function applyTagFilters() {
        tagCurrentPage = 1; // 重置为第一页
        fetchTags();
    }
    
    // 标签页面导航
    function navigateTagPage(page) {
        if (page < 1 || page > tagTotalPages) return;
        tagCurrentPage = page;
        fetchTags();
    }

    // 打开创建标签模态框
    function openCreateTagModal() {
        if (!elements.modalTitle || !elements.tagForm || !elements.tagModal) return;
        
        elements.modalTitle.textContent = '添加标签';
        elements.tagForm.reset();
        tagCurrentId = null;
        
        elements.tagModal.style.display = 'block';
    }

    // 关闭标签模态框
    function closeTagModal() {
        if (elements.tagModal) elements.tagModal.style.display = 'none';
    }

    // 处理标签表单提交
    function handleTagSubmit(e) {
        e.preventDefault();
        
        // 获取表单数据
        const nameField = document.getElementById('tagName');
        const descriptionField = document.getElementById('tagDescription');
        
        if (!nameField) {
            showNotification('表单数据不完整', 'error');
            return;
        }
        
        // 处理表单数据
        const formData = {
            name: nameField.value.trim(),
            description: descriptionField ? descriptionField.value.trim() : ''
        };
        
        // 表单验证
        if (!formData.name) {
            showNotification('标签名称不能为空', 'error');
            return;
        }
        
        // 创建标签
        fetch('/admins/tags', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(formData)
        })
            .then(response => {
                if (!response.ok) {
                    // 尝试解析错误消息
                    return response.json().then(err => {
                        throw new Error(err.message || '操作失败');
                    });
                }
                return response.json();
            })
            .then(() => {
                closeTagModal();
                fetchTags(); // 刷新标签列表
                
                // 显示操作成功的通知
                showNotification('标签已成功创建', 'success');
            })
            .catch(error => {
                console.error('保存标签失败:', error);
                showNotification(`保存标签失败: ${error.message || '请稍后重试'}`, 'error');
            });
    }

    // 打开删除确认对话框
    function openDeleteConfirmation(tagId) {
        tagCurrentId = tagId;
        if (elements.deleteModal) elements.deleteModal.style.display = 'block';
    }

    // 关闭删除确认对话框
    function closeDeleteModal() {
        if (elements.deleteModal) elements.deleteModal.style.display = 'none';
    }

    // 删除标签
    function deleteTag() {
        if (!tagCurrentId) return;
        
        // 禁用删除按钮，防止重复点击
        if (elements.confirmDeleteBtn) {
            elements.confirmDeleteBtn.disabled = true;
            elements.confirmDeleteBtn.textContent = '删除中...';
        }
        
        fetch(`/admins/tags/${tagCurrentId}`, {
            method: 'DELETE'
        })
            .then(response => {
                if (!response.ok) {
                    // 尝试解析错误消息
                    return response.json().then(err => {
                        throw new Error(err.message || '删除失败');
                    });
                }
                return response.json();
            })
            .then(() => {
                closeDeleteModal();
                fetchTags(); // 刷新标签列表
                showNotification('标签已成功删除', 'success');
            })
            .catch(error => {
                console.error('删除标签失败:', error);
                showNotification(`删除标签失败: ${error.message || '请稍后重试'}`, 'error');
            })
            .finally(() => {
                // 恢复按钮状态
                if (elements.confirmDeleteBtn) {
                    elements.confirmDeleteBtn.disabled = false;
                    elements.confirmDeleteBtn.textContent = '删除';
                }
            });
    }
    
    // 显示操作通知
    function showNotification(message, type = 'info') {
        // 检查是否已存在通知容器，如果不存在则创建
        let notificationContainer = document.getElementById('notification-container');
        if (!notificationContainer) {
            notificationContainer = document.createElement('div');
            notificationContainer.id = 'notification-container';
            document.body.appendChild(notificationContainer);
        }
        
        // 创建通知元素
        const notification = document.createElement('div');
        notification.className = `notification notification-${type}`;
        notification.innerHTML = `
            <div class="notification-content">
                ${message}
            </div>
            <span class="notification-close">&times;</span>
        `;
        
        // 添加到容器
        notificationContainer.appendChild(notification);
        
        // 淡入效果
        setTimeout(() => {
            notification.style.opacity = '1';
        }, 10);
        
        // 关闭按钮
        const closeBtn = notification.querySelector('.notification-close');
        if (closeBtn) {
            closeBtn.addEventListener('click', () => {
                closeNotification(notification);
            });
        }
        
        // 几秒后自动关闭
        setTimeout(() => {
            closeNotification(notification);
        }, 4000);
    }
    
    // 关闭通知
    function closeNotification(notification) {
        notification.style.opacity = '0';
        setTimeout(() => {
            if (notification.parentNode) {
                notification.parentNode.removeChild(notification);
            }
        }, 300);
    }

    // 公开接口
    return {
        init: init
    };
})();

// 当标签管理标签被点击时调用初始化函数
document.addEventListener('DOMContentLoaded', function() {
    const tagTabTrigger = document.querySelector('[data-tab="tags"]');
    if (tagTabTrigger) {
        tagTabTrigger.addEventListener('click', function() {
            // 给初始化函数一点时间，确保DOM已完全渲染
            setTimeout(TagManager.init, 100);
        });
    }
});