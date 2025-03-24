document.addEventListener('DOMContentLoaded', function() {
    // 全局变量
    let articleId = null;
    let isLiked = false;
    let isBookmarked = false;
    
    // 初始化
    init();
    
    function init() {
        setupEventListeners();
        loadArticleDetails();
        checkAuthStatus();
        setupDarkMode();
    }
    
    // 设置事件监听器
    function setupEventListeners() {
        // 导航菜单切换
        const mobileToggle = document.querySelector('.mobile-toggle');
        const mainNav = document.querySelector('.main-nav');
        
        if (mobileToggle) {
            mobileToggle.addEventListener('click', function() {
                mainNav.classList.toggle('active');
                this.classList.toggle('active');
            });
        }
        
        // 用户头像点击事件
        const userAvatar = document.getElementById('userAvatar');
        const userDropdown = document.querySelector('.user-dropdown');
        
        if (userAvatar && userDropdown) {
            userAvatar.addEventListener('click', function(e) {
                e.stopPropagation();
                userDropdown.classList.toggle('show');
            });
            
            document.addEventListener('click', function() {
                if (userDropdown.classList.contains('show')) {
                    userDropdown.classList.remove('show');
                }
            });
        }
        
        // 退出登录
        const logoutBtn = document.getElementById('logout');
        if (logoutBtn) {
            logoutBtn.addEventListener('click', function(e) {
                e.preventDefault();
                logout();
            });
        }
        
        // 点赞按钮
        const likeBtn = document.getElementById('likeBtn');
        if (likeBtn) {
            likeBtn.addEventListener('click', function() {
                toggleLike();
            });
        }
        
        // 收藏按钮
        const bookmarkBtn = document.getElementById('bookmarkBtn');
        if (bookmarkBtn) {
            bookmarkBtn.addEventListener('click', function() {
                toggleBookmark();
            });
        }
        
        // 分享按钮
        const shareBtn = document.getElementById('shareBtn');
        if (shareBtn) {
            shareBtn.addEventListener('click', function() {
                shareArticle();
            });
        }
        
        // 评论提交
        const submitComment = document.getElementById('submitComment');
        if (submitComment) {
            submitComment.addEventListener('click', function() {
                submitNewComment();
            });
        }
        
        // 回到顶部按钮
        const backToTopBtn = document.getElementById('backToTop');
        if (backToTopBtn) {
            window.addEventListener('scroll', function() {
                if (window.pageYOffset > 300) {
                    backToTopBtn.classList.add('visible');
                } else {
                    backToTopBtn.classList.remove('visible');
                }
            });
            
            backToTopBtn.addEventListener('click', function() {
                window.scrollTo({
                    top: 0,
                    behavior: 'smooth'
                });
            });
        }
        
        // 主题切换
        const themeToggle = document.getElementById('themeToggle');
        if (themeToggle) {
            themeToggle.addEventListener('click', function() {
                toggleDarkMode();
            });
        }
        
        // 监听滚动事件，更新目录高亮
        window.addEventListener('scroll', function() {
            updateTocHighlight();
        });
    }
    
    // 加载文章详情
    function loadArticleDetails() {
        // 获取URL中的文章ID参数
        const urlParams = new URLSearchParams(window.location.search);
        articleId = urlParams.get('id');
        
        if (!articleId) {
            showNotification('文章ID不存在，无法加载文章', 'error');
            return;
        }
        
        showLoading();
        
        // 发送请求获取文章详情
        fetch(`/api/article/${articleId}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                if (data.article) {
                    renderArticle(data.article);
                    
                    // 加载评论
                    loadComments(articleId);
                    
                    // 加载相关文章
                    loadRelatedArticles(data.article.category, articleId);
                    
                    // 生成目录
                    generateTableOfContents();
                    
                    // 检查用户是否已点赞或收藏
                    checkUserInteractions();
                } else {
                    throw new Error('文章不存在');
                }
                hideLoading();
            })
            .catch(error => {
                console.error('获取文章详情失败:', error);
                // 使用模拟数据作为后备方案
                const mockArticle = generateMockArticle(articleId);
                renderArticle(mockArticle);
                
                // 生成模拟评论
                renderComments(generateMockComments());
                
                // 生成模拟相关文章
                renderRelatedArticles(generateMockRelatedArticles());
                
                // 生成目录
                generateTableOfContents();
                
                hideLoading();
                showNotification('获取文章详情失败，显示模拟数据', 'error');
            });
    }
    
    // 渲染文章内容
    function renderArticle(article) {
        document.title = `${article.title} - 云中杉木博客`;
        
        // 设置文章头图
        const postImage = document.getElementById('postImage');
        const postHeader = document.querySelector('.post-header');
        
        if (article.thumbnail && article.thumbnail.trim() !== '') {
            // 有缩略图
            postImage.src = article.thumbnail;
            postImage.alt = article.title;
            postImage.style.display = 'block';
            postHeader.classList.remove('no-image');
        } else {
            // 无缩略图
            postImage.style.display = 'none';
            postHeader.classList.add('no-image');
        }
        
        // 设置分类
        const postCategory = document.getElementById('postCategory');
        postCategory.textContent = article.category;
        
        // 设置标题
        const postTitle = document.getElementById('postTitle');
        postTitle.textContent = article.title;
        
        // 设置日期
        const postDate = document.getElementById('postDate');
        postDate.textContent = formatDate(article.date);
        
        // 设置作者信息
        const authorAvatar = document.getElementById('authorAvatar');
        const authorName = document.getElementById('authorName');
        // 如果作者头像为空，使用默认头像
        authorAvatar.src = article.authorAvatar || 'img/default_touxiang.jpg';
        authorName.textContent = article.author;
        
        // 设置侧边栏作者信息
        const sidebarAuthorAvatar = document.getElementById('sidebarAuthorAvatar');
        const sidebarAuthorName = document.getElementById('sidebarAuthorName');
        const authorBio = document.getElementById('authorBio');
        // 如果作者头像为空，使用默认头像
        sidebarAuthorAvatar.src = article.authorAvatar || 'img/default_touxiang.jpg';
        sidebarAuthorName.textContent = article.author;
        authorBio.textContent = article.authorBio || '这位作者暂未添加个人简介';
        
        // 设置统计信息
        const viewCount = document.getElementById('viewCount');
        const likeCount = document.getElementById('likeCount');
        const commentCount = document.getElementById('commentCount');
        const commentCountDisplay = document.getElementById('commentCountDisplay');
        viewCount.textContent = article.views;
        likeCount.textContent = article.likes;
        commentCount.textContent = article.comments;
        commentCountDisplay.textContent = article.comments;
        
        // 设置文章内容
        const postContent = document.getElementById('postContent');
        postContent.innerHTML = article.content;
        
        // 设置标签
        const postTags = document.getElementById('postTags');
        if (article.tags && article.tags.length > 0) {
            let tagsHTML = '';
            article.tags.forEach(tag => {
                tagsHTML += `<a href="new_blog_categories.html?tag=${encodeURIComponent(tag)}" class="post-tag">${tag}</a>`;
            });
            postTags.innerHTML = tagsHTML;
        } else {
            postTags.innerHTML = '<span class="post-tag">暂无标签</span>';
        }
        
        // 设置上一篇/下一篇文章
        const prevPost = document.getElementById('prevPost');
        const nextPost = document.getElementById('nextPost');
        
        // 移除可能已经绑定的所有点击事件
        prevPost.onclick = null;
        nextPost.onclick = null;
        
        // 检查prevPost是否有效（ID大于0）
        if (article.prevPost && article.prevPost.id > 0 && article.prevPost.title) {
            prevPost.removeAttribute('onclick');
            prevPost.href = `blog_detail.html?id=${article.prevPost.id}`;
            prevPost.querySelector('.post-navigation-title').textContent = article.prevPost.title;
            prevPost.classList.remove('disabled');
            
            // 添加自定义点击事件
            prevPost.onclick = function(e) {
                e.preventDefault();
                handleArticleNavigation(article.prevPost.id);
                return false;
            };
        } else {
            prevPost.removeAttribute('href');
            prevPost.classList.add('disabled');
            prevPost.querySelector('.post-navigation-title').textContent = '已经是第一篇文章了';
            
            // 为禁用按钮添加点击事件
            prevPost.onclick = function(e) {
                e.preventDefault();
                showNotification('已经是第一篇文章了', 'info');
                return false;
            };
        }
        
        // 检查nextPost是否有效（ID大于0）
        if (article.nextPost && article.nextPost.id > 0 && article.nextPost.title) {
            nextPost.removeAttribute('onclick');
            nextPost.href = `blog_detail.html?id=${article.nextPost.id}`;
            nextPost.querySelector('.post-navigation-title').textContent = article.nextPost.title;
            nextPost.classList.remove('disabled');
            
            // 添加自定义点击事件
            nextPost.onclick = function(e) {
                e.preventDefault();
                handleArticleNavigation(article.nextPost.id);
                return false;
            };
        } else {
            nextPost.removeAttribute('href');
            nextPost.classList.add('disabled');
            nextPost.querySelector('.post-navigation-title').textContent = '已经是最后一篇文章了';
            
            // 为禁用按钮添加点击事件
            nextPost.onclick = function(e) {
                e.preventDefault();
                showNotification('已经是最后一篇文章了', 'info');
                return false;
            };
        }
    }
    
    // 加载评论
    function loadComments(articleId) {
        fetch(`/api/comments?articleId=${articleId}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                renderComments(data.comments || []);
            })
            .catch(error => {
                console.error('获取评论失败:', error);
                // 使用模拟数据作为后备方案
                renderComments(generateMockComments());
            });
    }
    
    // 渲染评论
    function renderComments(comments) {
        const commentsList = document.getElementById('commentsList');
        const commentCount = document.getElementById('commentCount');
        const commentCountDisplay = document.getElementById('commentCountDisplay');
        
        if (comments.length === 0) {
            commentsList.innerHTML = '<div class="no-comments">暂无评论，快来发表第一条评论吧！</div>';
            commentCount.textContent = '0';
            commentCountDisplay.textContent = '0';
            return;
        }
        
        let commentsHTML = '';
        comments.forEach(comment => {
            commentsHTML += `
            <div class="comment" data-id="${comment.id}">
                <img src="${comment.avatar}" alt="${comment.username}" class="comment-avatar">
                <div class="comment-body">
                    <div class="comment-header">
                        <span class="comment-author">${comment.username}</span>
                        <span class="comment-date">${formatDate(comment.date)}</span>
                    </div>
                    <div class="comment-text">${comment.content}</div>
                    <div class="comment-actions">
                        <span class="comment-action comment-like" data-id="${comment.id}">
                            <i class="far fa-heart"></i> ${comment.likes || 0}
                        </span>
                        <span class="comment-action comment-reply" data-id="${comment.id}">
                            <i class="far fa-comment"></i> 回复
                        </span>
                    </div>
                </div>
            </div>
            `;
        });
        
        commentsList.innerHTML = commentsHTML;
        commentCount.textContent = comments.length;
        commentCountDisplay.textContent = comments.length;
        
        // 添加评论点赞和回复事件
        const commentLikeButtons = document.querySelectorAll('.comment-like');
        const commentReplyButtons = document.querySelectorAll('.comment-reply');
        
        commentLikeButtons.forEach(button => {
            button.addEventListener('click', function() {
                const commentId = this.getAttribute('data-id');
                likeComment(commentId);
            });
        });
        
        commentReplyButtons.forEach(button => {
            button.addEventListener('click', function() {
                const commentId = this.getAttribute('data-id');
                const authorName = this.closest('.comment').querySelector('.comment-author').textContent;
                replyToComment(commentId, authorName);
            });
        });
    }
    
    // 加载相关文章
    function loadRelatedArticles(category, currentArticleId) {
        fetch(`/api/article/related?category=${encodeURIComponent(category)}&excludeId=${currentArticleId}&size=5`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                renderRelatedArticles(data.articles || []);
            })
            .catch(error => {
                console.error('获取相关文章失败:', error);
                // 使用模拟数据作为后备方案
                renderRelatedArticles(generateMockRelatedArticles());
            });
    }
    
    // 渲染相关文章
    function renderRelatedArticles(articles) {
        const relatedPosts = document.getElementById('relatedPosts');
        
        if (articles.length === 0) {
            relatedPosts.innerHTML = '<p>暂无相关文章</p>';
            return;
        }
        
        let html = '';
        articles.forEach((post, index) => {
            // 检查缩略图是否为空，如果为空则使用随机图片
            const thumbnail = post.thumbnail ? post.thumbnail : `https://picsum.photos/300/200?random=${post.id || index}`;
            
            html += `
            <li class="popular-post">
                <img src="${thumbnail}" alt="${post.title}">
                <div class="popular-post-info">
                    <h4><a href="blog_detail.html?id=${post.id}">${post.title}</a></h4>
                    <div class="meta">
                        <span><i class="fas fa-eye"></i> ${post.views}</span>
                        <span><i class="fas fa-heart"></i> ${post.likes}</span>
                    </div>
                </div>
            </li>
            `;
        });
        
        relatedPosts.innerHTML = html;
    }
    
    // 生成文章目录
    function generateTableOfContents() {
        const content = document.getElementById('postContent');
        const tocList = document.getElementById('tocList');
        
        if (!content || !tocList) return;
        
        // 获取所有标题元素
        const headings = content.querySelectorAll('h2, h3, h4');
        
        if (headings.length === 0) {
            document.getElementById('tableOfContents').style.display = 'none';
            return;
        }
        
        // 为每个标题添加id
        headings.forEach((heading, index) => {
            if (!heading.id) {
                heading.id = `heading-${index}`;
            }
        });
        
        // 生成目录HTML
        let tocHTML = '';
        headings.forEach(heading => {
            const level = parseInt(heading.tagName.charAt(1));
            tocHTML += `
            <li class="toc-item level-${level}">
                <a href="#${heading.id}">${heading.textContent}</a>
            </li>
            `;
        });
        
        tocList.innerHTML = tocHTML;
        
        // 初始化目录高亮
        updateTocHighlight();
    }
    
    // 更新目录高亮
    function updateTocHighlight() {
        const content = document.getElementById('postContent');
        const tocItems = document.querySelectorAll('.toc-item');
        
        if (!content || tocItems.length === 0) return;
        
        // 获取所有标题元素
        const headings = content.querySelectorAll('h2, h3, h4');
        
        // 找到当前可见的标题
        let currentHeadingId = null;
        const scrollPosition = window.scrollY + 150; // 偏移量
        
        for (let i = 0; i < headings.length; i++) {
            if (headings[i].offsetTop <= scrollPosition) {
                currentHeadingId = headings[i].id;
            } else {
                break;
            }
        }
        
        // 更新目录高亮
        tocItems.forEach(item => {
            item.classList.remove('active');
            const link = item.querySelector('a');
            const href = link.getAttribute('href').substring(1);
            
            if (href === currentHeadingId) {
                item.classList.add('active');
            }
        });
    }
    
    // 点赞文章
    function toggleLike() {
        // 检查用户是否已登录
        const userData = localStorage.getItem('userInfo');
        if (!userData) {
            // 游客点赞时弹出提示，并提供登录链接
            showLoginNotification('请先登录后再点赞', 'warning');
            return;
        }
        
        const likeBtn = document.getElementById('likeBtn');
        const likeCount = document.getElementById('likeCount');
        
        // 切换点赞状态
        isLiked = !isLiked;
        
        // 更新UI
        if (isLiked) {
            likeBtn.innerHTML = '<i class="fas fa-heart"></i> 已点赞';
            likeBtn.classList.add('liked');
            likeCount.textContent = parseInt(likeCount.textContent) + 1;
        } else {
            likeBtn.innerHTML = '<i class="far fa-heart"></i> 点赞';
            likeBtn.classList.remove('liked');
            likeCount.textContent = parseInt(likeCount.textContent) - 1;
        }
        
        // 发送请求更新点赞状态
        fetch(`/api/article/like/${articleId}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                liked: isLiked
            }),
            credentials: 'include'
        })
        .then(response => {
            if (!response.ok) {
                if (response.status === 401) {
                    // 服务器返回未授权状态码时
                    throw new Error('未登录或登录已过期');
                }
                throw new Error('操作失败');
            }
            return response.json();
        })
        .then(data => {
            // 服务器确认成功后显示成功消息
            showNotification(isLiked ? '点赞成功' : '已取消点赞', 'success');
        })
        .catch(error => {
            console.error('更新点赞状态失败:', error);
            
            if (error.message === '未登录或登录已过期') {
                // 登录已过期的情况
                localStorage.removeItem('userInfo');
                showLoginNotification('登录已过期，请重新登录后再点赞', 'error');
            } else {
                showNotification('操作失败，请重试', 'error');
            }
            
            // 恢复原状态
            isLiked = !isLiked;
            if (isLiked) {
                likeBtn.innerHTML = '<i class="fas fa-heart"></i> 已点赞';
                likeBtn.classList.add('liked');
                likeCount.textContent = parseInt(likeCount.textContent) + 1;
            } else {
                likeBtn.innerHTML = '<i class="far fa-heart"></i> 点赞';
                likeBtn.classList.remove('liked');
                likeCount.textContent = parseInt(likeCount.textContent) - 1;
            }
        });
    }
    
    // 添加一个新的函数，显示带有登录按钮的通知
    function showLoginNotification(message, type = 'info') {
        // 先删除可能存在的通知
        const existingNotification = document.querySelector('.notification');
        if (existingNotification) {
            existingNotification.remove();
        }
        
        // 创建新通知
        const notification = document.createElement('div');
        notification.className = `notification ${type} with-button`;
        
        let icon = '';
        switch (type) {
            case 'success':
                icon = '<i class="fas fa-check-circle"></i>';
                break;
            case 'error':
                icon = '<i class="fas fa-exclamation-circle"></i>';
                break;
            case 'warning':
                icon = '<i class="fas fa-exclamation-triangle"></i>';
                break;
            default:
                icon = '<i class="fas fa-info-circle"></i>';
        }
        
        notification.innerHTML = `
            ${icon}
            <div class="notification-message">${message}</div>
            <a href="blog_login.html" class="notification-login-btn">立即登录</a>
            <div class="notification-close"><i class="fas fa-times"></i></div>
        `;
        
        document.body.appendChild(notification);
        
        // 显示通知
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);
        
        // 添加关闭事件
        const closeBtn = notification.querySelector('.notification-close');
        closeBtn.addEventListener('click', () => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        });
        
        // 自动关闭（时间更长，让用户有足够时间点击登录按钮）
        setTimeout(() => {
            if (document.body.contains(notification)) {
                notification.classList.remove('show');
                setTimeout(() => {
                    notification.remove();
                }, 300);
            }
        }, 8000);
    }
    
    // 收藏文章
    function toggleBookmark() {
        // 检查用户是否已登录
        const userData = localStorage.getItem('userInfo');
        if (!userData) {
            showLoginNotification('请先登录后再收藏', 'warning');
            return;
        }
        
        const bookmarkBtn = document.getElementById('bookmarkBtn');
        
        // 切换收藏状态
        isBookmarked = !isBookmarked;
        
        // 更新UI
        if (isBookmarked) {
            bookmarkBtn.innerHTML = '<i class="fas fa-bookmark"></i> 已收藏';
            bookmarkBtn.classList.add('bookmarked');
        } else {
            bookmarkBtn.innerHTML = '<i class="far fa-bookmark"></i> 收藏';
            bookmarkBtn.classList.remove('bookmarked');
        }
        
        // 发送请求更新收藏状态
        fetch(`/api/article/bookmark/${articleId}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                bookmarked: isBookmarked
            }),
            credentials: 'include'
        })
        .catch(error => {
            console.error('更新收藏状态失败:', error);
            showNotification('操作失败，请重试', 'error');
            
            // 恢复原状态
            isBookmarked = !isBookmarked;
            if (isBookmarked) {
                bookmarkBtn.innerHTML = '<i class="fas fa-bookmark"></i> 已收藏';
                bookmarkBtn.classList.add('bookmarked');
            } else {
                bookmarkBtn.innerHTML = '<i class="far fa-bookmark"></i> 收藏';
                bookmarkBtn.classList.remove('bookmarked');
            }
        });
    }
    
    // 分享文章
    function shareArticle() {
        // 如果浏览器支持网页分享API
        if (navigator.share) {
            navigator.share({
                title: document.title,
                url: window.location.href
            })
            .then(() => console.log('分享成功'))
            .catch(error => console.log('分享失败:', error));
        } else {
            // 复制链接到剪贴板
            const tempInput = document.createElement('input');
            tempInput.value = window.location.href;
            document.body.appendChild(tempInput);
            tempInput.select();
            document.execCommand('copy');
            document.body.removeChild(tempInput);
            
            showNotification('链接已复制到剪贴板', 'success');
        }
    }
    
    // 评论点赞
    function likeComment(commentId) {
        // 检查用户是否已登录
        const userData = localStorage.getItem('userInfo');
        if (!userData) {
            showLoginNotification('请先登录后再点赞评论', 'warning');
            return;
        }
        
        // 更新UI
        const likeButton = document.querySelector(`.comment-like[data-id="${commentId}"]`);
        const currentLikes = parseInt(likeButton.textContent.trim());
        
        if (likeButton.classList.contains('liked')) {
            likeButton.classList.remove('liked');
            likeButton.innerHTML = `<i class="far fa-heart"></i> ${currentLikes - 1}`;
        } else {
            likeButton.classList.add('liked');
            likeButton.innerHTML = `<i class="fas fa-heart"></i> ${currentLikes + 1}`;
        }
        
        // 发送请求更新评论点赞状态
        fetch(`/api/comments/like/${commentId}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                liked: !likeButton.classList.contains('liked')
            }),
            credentials: 'include'
        })
        .catch(error => {
            console.error('更新评论点赞状态失败:', error);
            showNotification('操作失败，请重试', 'error');
            
            // 恢复原状态
            if (likeButton.classList.contains('liked')) {
                likeButton.classList.remove('liked');
                likeButton.innerHTML = `<i class="far fa-heart"></i> ${currentLikes}`;
            } else {
                likeButton.classList.add('liked');
                likeButton.innerHTML = `<i class="fas fa-heart"></i> ${currentLikes}`;
            }
        });
    }
    
    // 回复评论
    function replyToComment(commentId, authorName) {
        // 检查用户是否已登录
        const userData = localStorage.getItem('userInfo');
        if (!userData) {
            showLoginNotification('请先登录后再回复评论', 'warning');
            return;
        }
        
        const commentInput = document.getElementById('commentInput');
        commentInput.value = `@${authorName} `;
        commentInput.focus();
        
        // 滚动到评论框
        commentInput.scrollIntoView({ behavior: 'smooth' });
    }
    
    // 提交新评论
    function submitNewComment() {
        // 检查用户是否已登录
        const userData = localStorage.getItem('userInfo');
        if (!userData) {
            showLoginNotification('请先登录后再发表评论', 'warning');
            return;
        }
        
        const commentInput = document.getElementById('commentInput');
        const commentContent = commentInput.value.trim();
        
        if (commentContent === '') {
            showNotification('评论内容不能为空', 'error');
            return;
        }
        
        // 显示加载状态
        const submitBtn = document.getElementById('submitComment');
        submitBtn.disabled = true;
        submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 提交中...';
        
        // 解析用户信息
        const user = JSON.parse(userData);
        
        // 发送请求提交评论
        fetch(`/api/comments`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                articleId: articleId,
                content: commentContent
            }),
            credentials: 'include'
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('提交评论失败');
            }
            return response.json();
        })
        .then(data => {
            // 清空输入框
            commentInput.value = '';
            
            // 添加新评论到列表
            const commentsList = document.getElementById('commentsList');
            const noComments = commentsList.querySelector('.no-comments');
            
            if (noComments) {
                commentsList.innerHTML = '';
            }
            
            const newComment = document.createElement('div');
            newComment.className = 'comment';
            newComment.setAttribute('data-id', data.id);
            newComment.innerHTML = `
            <img src="${user.avatar || 'img/default_touxiang.jpg'}" alt="${user.username}" class="comment-avatar">
            <div class="comment-body">
                <div class="comment-header">
                    <span class="comment-author">${user.username}</span>
                    <span class="comment-date">刚刚</span>
                </div>
                <div class="comment-text">${commentContent}</div>
                <div class="comment-actions">
                    <span class="comment-action comment-like" data-id="${data.id}">
                        <i class="far fa-heart"></i> 0
                    </span>
                    <span class="comment-action comment-reply" data-id="${data.id}">
                        <i class="far fa-comment"></i> 回复
                    </span>
                </div>
            </div>
            `;
            
            commentsList.prepend(newComment);
            
            // 更新评论数量
            const commentCount = document.getElementById('commentCount');
            const commentCountDisplay = document.getElementById('commentCountDisplay');
            const newCount = parseInt(commentCount.textContent) + 1;
            commentCount.textContent = newCount;
            commentCountDisplay.textContent = newCount;
            
            showNotification('评论发表成功', 'success');
            
            // 添加评论点赞和回复事件
            const newLikeButton = newComment.querySelector('.comment-like');
            const newReplyButton = newComment.querySelector('.comment-reply');
            
            newLikeButton.addEventListener('click', function() {
                const commentId = this.getAttribute('data-id');
                likeComment(commentId);
            });
            
            newReplyButton.addEventListener('click', function() {
                const commentId = this.getAttribute('data-id');
                const authorName = this.closest('.comment').querySelector('.comment-author').textContent;
                replyToComment(commentId, authorName);
            });
        })
        .catch(error => {
            console.error('提交评论失败:', error);
            showNotification('评论发表失败，请重试', 'error');
        })
        .finally(() => {
            // 恢复按钮状态
            submitBtn.disabled = false;
            submitBtn.textContent = '发表评论';
        });
    }
    
    // 检查用户与文章的互动状态（点赞、收藏）
    function checkUserInteractions() {
        // 检查用户是否已登录
        const userData = localStorage.getItem('userInfo');
        if (!userData) return;
        
        // 发送请求获取用户与当前文章的互动状态
        fetch(`/api/article/interactions/${articleId}`, {
            credentials: 'include'
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('获取互动状态失败');
            }
            return response.json();
        })
        .then(data => {
            // 更新点赞状态
            isLiked = data.liked || false;
            const likeBtn = document.getElementById('likeBtn');
            
            if (isLiked) {
                likeBtn.innerHTML = '<i class="fas fa-heart"></i> 已点赞';
                likeBtn.classList.add('liked');
            }
            
            // 更新收藏状态
            isBookmarked = data.bookmarked || false;
            const bookmarkBtn = document.getElementById('bookmarkBtn');
            
            if (isBookmarked) {
                bookmarkBtn.innerHTML = '<i class="fas fa-bookmark"></i> 已收藏';
                bookmarkBtn.classList.add('bookmarked');
            }
        })
        .catch(error => {
            console.error('获取互动状态失败:', error);
        });
    }
    
    // 检查登录状态
    function checkAuthStatus() {
        // 发送请求到后端检查用户登录状态
        fetch('/api/user/info')
            .then(response => {
                if (!response.ok) {
                    if (response.status === 401) {
                        // 用户未登录，使用游客状态
                        updateUIForAuthStatus(false);
                        return null;
                    }
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                if (data) {
                    // 用户已登录，更新UI显示用户信息
                    updateUIForAuthStatus(true, data);
                }
            })
            .catch(error => {
                console.error('检查登录状态失败:', error);
                // 默认使用未登录状态
                updateUIForAuthStatus(false);
            });
    }
    
    // 更新UI显示用户信息
    function updateUIForAuthStatus(isLoggedIn, userData) {
        const userAvatar = document.getElementById('userAvatar');
        const userDropdown = document.querySelector('.user-dropdown');
        
        if (isLoggedIn && userData) {
            // 用户已登录，显示用户信息
            if (userAvatar) userAvatar.src = userData.avatar || 'img/default_touxiang.jpg';
            
            // 更新下拉菜单
            if (userDropdown) {
                userDropdown.innerHTML = `
                <a href="user_center.html"><i class="fas fa-user-circle"></i> 个人中心</a>
                <a href="blog_editor.html"><i class="fas fa-edit"></i> 写博客</a>
                <a href="user_settings.html"><i class="fas fa-cog"></i> 设置</a>
                <a href="#" id="logout"><i class="fas fa-sign-out-alt"></i> 退出登录</a>
                `;
                
                // 重新绑定退出登录事件
                const logoutBtn = document.getElementById('logout');
                if (logoutBtn) {
                    logoutBtn.addEventListener('click', function(e) {
                        e.preventDefault();
                        logout();
                    });
                }
            }
        } else {
            // 用户未登录，显示游客信息
            if (userAvatar) userAvatar.src = 'img/default_touxiang.jpg';
            
            // 更新下拉菜单
            if (userDropdown) {
                userDropdown.innerHTML = `
                <a href="blog_login.html"><i class="fas fa-sign-in-alt"></i> 登录</a>
                <a href="blog_register.html"><i class="fas fa-user-plus"></i> 注册</a>
                `;
            }
        }
    }
    
    // 退出登录
    function logout() {
        // 发送请求到后端进行退出登录操作
        fetch('/api/user/logout', {
            method: 'POST',
            credentials: 'same-origin' // 确保发送cookies
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('退出登录失败');
            }
            return response.json();
        })
        .then(data => {
            showNotification(data.message || '已成功退出登录', 'success');
            // 清除本地存储的用户信息
            localStorage.removeItem('userInfo');
            // 更新UI为未登录状态
            updateUIForAuthStatus(false);
        })
        .catch(error => {
            console.error('退出登录失败:', error);
            showNotification('退出登录请求失败，请重试', 'error');
        });
    }
    
    // 设置暗黑模式
    function setupDarkMode() {
        // 检查本地存储中的主题偏好
        const isDarkMode = localStorage.getItem('darkMode') === 'true';
        if (isDarkMode) {
            document.body.classList.add('dark-theme');
            updateDarkModeIcon(true);
        }
    }
    
    // 切换暗黑模式
    function toggleDarkMode() {
        const isDarkMode = document.body.classList.toggle('dark-theme');
        localStorage.setItem('darkMode', isDarkMode);
        updateDarkModeIcon(isDarkMode);
        showNotification(`已切换到${isDarkMode ? '暗色' : '亮色'}模式`, 'success');
    }
    
    // 更新暗黑模式图标
    function updateDarkModeIcon(isDarkMode) {
        const themeToggle = document.getElementById('themeToggle');
        if (themeToggle) {
            themeToggle.innerHTML = isDarkMode ? 
                '<i class="fas fa-sun"></i>' : 
                '<i class="fas fa-moon"></i>';
        }
    }
    
    // 显示加载中状态
    function showLoading() {
        const postContainer = document.getElementById('postContainer');
        if (!postContainer) return;
        
        // 保留现有的加载状态
    }
    
    // 隐藏加载中状态
    function hideLoading() {
        // 加载完成后，已经渲染了实际内容，所以不需要特别处理
    }
    
    // 显示通知消息
    function showNotification(message, type = 'info') {
        // 先删除可能存在的通知
        const existingNotification = document.querySelector('.notification');
        if (existingNotification) {
            existingNotification.remove();
        }
        
        // 创建新通知
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        
        let icon = '';
        switch (type) {
            case 'success':
                icon = '<i class="fas fa-check-circle"></i>';
                break;
            case 'error':
                icon = '<i class="fas fa-exclamation-circle"></i>';
                break;
            default:
                icon = '<i class="fas fa-info-circle"></i>';
        }
        
        notification.innerHTML = `
            ${icon}
            <div class="notification-message">${message}</div>
            <div class="notification-close"><i class="fas fa-times"></i></div>
        `;
        
        document.body.appendChild(notification);
        
        // 显示通知
        setTimeout(() => {
            notification.classList.add('show');
        }, 10);
        
        // 添加关闭事件
        const closeBtn = notification.querySelector('.notification-close');
        closeBtn.addEventListener('click', () => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        });
        
        // 自动关闭
        setTimeout(() => {
            notification.classList.remove('show');
            setTimeout(() => {
                notification.remove();
            }, 300);
        }, 5000);
    }
    
    // 格式化日期
    function formatDate(dateString) {
        const date = new Date(dateString);
        
        // 如果日期无效，返回原始字符串
        if (isNaN(date.getTime())) {
            return dateString;
        }
        
        const now = new Date();
        const diff = Math.floor((now - date) / 1000); // 差异（秒）
        
        if (diff < 60) {
            return '刚刚';
        } else if (diff < 3600) {
            return Math.floor(diff / 60) + '分钟前';
        } else if (diff < 86400) {
            return Math.floor(diff / 3600) + '小时前';
        } else if (diff < 2592000) {
            return Math.floor(diff / 86400) + '天前';
        } else {
            const year = date.getFullYear();
            const month = String(date.getMonth() + 1).padStart(2, '0');
            const day = String(date.getDate()).padStart(2, '0');
            return `${year}-${month}-${day}`;
        }
    }
    
    // 生成模拟文章
    function generateMockArticle(id) {
        const authors = [
            { name: '张三', avatar: 'img/avatar1.jpg', bio: '前端开发工程师，热爱写作和分享技术经验。' },
            { name: '李四', avatar: 'img/avatar2.jpg', bio: '全栈开发者，有5年工作经验，专注于Web应用开发。' },
            { name: '王五', avatar: 'img/avatar3.jpg', bio: '后端架构师，对高性能服务器架构有深入研究。' }
        ];
        
        const categories = ['技术', '前端', '后端', '数据库', '算法', '生活感悟'];
        const tags = ['JavaScript', 'HTML', 'CSS', 'React', 'Vue', 'Node.js', 'Python', 'Java', 'MySQL', '算法', '数据结构', '前端开发', '后端开发'];
        
        // 随机选择作者和分类
        const author = authors[Math.floor(Math.random() * authors.length)];
        const category = categories[Math.floor(Math.random() * categories.length)];
        
        // 随机生成3-5个标签
        const articleTags = [];
        const tagCount = Math.floor(Math.random() * 3) + 3;
        for (let i = 0; i < tagCount; i++) {
            const tag = tags[Math.floor(Math.random() * tags.length)];
            if (!articleTags.includes(tag)) {
                articleTags.push(tag);
            }
        }
        
        // 生成上一篇/下一篇文章链接
        const prevPostId = parseInt(id) - 1;
        const nextPostId = parseInt(id) + 1;
        
        // 生成文章内容
        const content = `
        <h2>引言</h2>
        <p>欢迎阅读这篇博客文章！这是一篇模拟生成的文章，用于展示博客详情页面的设计和功能。</p>
        
        <p>在这篇文章中，我们将探讨一些关于Web开发的重要概念和最佳实践。</p>
        
        <h2>第一部分：基础知识</h2>
        <p>在Web开发中，HTML、CSS和JavaScript构成了基础的三大技术栈。</p>
        
        <h3>HTML的重要性</h3>
        <p>HTML（超文本标记语言）是构建网页的基础。它提供了网页的基本结构和内容。</p>
        
        <p>一个良好的HTML结构应该具备以下特点：</p>
        <ul>
            <li>语义化：使用合适的标签表达内容的含义</li>
            <li>可访问性：考虑到所有用户的使用需求</li>
            <li>良好的SEO：有利于搜索引擎索引</li>
        </ul>
        
        <h3>CSS的作用</h3>
        <p>CSS（层叠样式表）用于控制网页的样式和布局。</p>
        
        <p>现代CSS已经非常强大，可以实现复杂的布局和动画效果：</p>
        <ul>
            <li>Flexbox和Grid布局</li>
            <li>CSS变量</li>
            <li>CSS动画和过渡</li>
        </ul>
        
        <h3>JavaScript的能力</h3>
        <p>JavaScript是一种功能强大的编程语言，使网页具有交互性和动态性。</p>
        
        <p>现代JavaScript生态系统包括：</p>
        <ul>
            <li>ES6+的新特性</li>
            <li>框架和库（React、Vue、Angular等）</li>
            <li>Node.js服务器端JavaScript</li>
        </ul>
        
        <h2>第二部分：进阶技巧</h2>
        <p>掌握了基础知识后，让我们来看一些进阶的Web开发技巧。</p>
        
        <h3>响应式设计</h3>
        <p>响应式设计是现代Web开发的标准实践，确保网站在不同设备上都能提供良好的用户体验。</p>
        
        <pre><code>/* 媒体查询示例 */
@media (max-width: 768px) {
.container {
flex-direction: column;
}
}</code></pre>
        
        <h3>性能优化</h3>
        <p>Web性能对用户体验至关重要。以下是一些关键的性能优化技巧：</p>
        <ul>
            <li>图片懒加载</li>
            <li>代码分割</li>
            <li>缓存策略</li>
            <li>服务器端渲染</li>
        </ul>
        
        <blockquote>
            <p>"性能不是一个功能，而是每个功能的基础要求。"</p>
        </blockquote>
        
        <h2>总结</h2>
        <p>Web开发是一个不断发展的领域，需要持续学习和实践。希望这篇文章能给你带来一些有用的信息和启发。</p>
        
        <p>感谢阅读！如有问题或建议，欢迎在下方评论区留言。</p>
        `;
        
        return {
            id: id,
            title: `博客文章标题 ${id}: 这是一个关于Web开发的深入探讨`,
            excerpt: '这是文章的摘要内容，通常显示文章的前几段文字。这只是一个示例文本，用于展示文章卡片的样式和布局效果。',
            content: content,
            thumbnail: `https://picsum.photos/1200/600?random=${id}`,
            author: author.name,
            authorAvatar: author.avatar,
            authorBio: author.bio,
            date: new Date(Date.now() - Math.floor(Math.random() * 30) * 24 * 60 * 60 * 1000).toISOString(),
            category: category,
            tags: articleTags,
            views: Math.floor(Math.random() * 1000),
            likes: Math.floor(Math.random() * 200),
            comments: Math.floor(Math.random() * 50),
            prevPost: prevPostId > 0 ? { id: prevPostId, title: `博客文章标题 ${prevPostId}: 上一篇文章的标题` } : null,
            nextPost: { id: nextPostId, title: `博客文章标题 ${nextPostId}: 下一篇文章的标题` }
        };
    }
    
    // 生成模拟评论
    function generateMockComments() {
        const commentCount = Math.floor(Math.random() * 10) + 1;
        const comments = [];
        
        const users = [
            { username: '用户A', avatar: 'img/avatar1.jpg' },
            { username: '用户B', avatar: 'img/avatar2.jpg' },
            { username: '用户C', avatar: 'img/avatar3.jpg' },
            { username: '用户D', avatar: 'img/default_touxiang.jpg' }
        ];
        
        const commentContents = [
            '写得很好，学习了！',
            '这篇文章内容丰富，讲解清晰，非常有帮助！',
            '我对这个主题很感兴趣，请问有没有更多相关的资料推荐？',
            '文章中提到的第二点我有些疑问，能否详细解释一下？',
            '这个观点很有启发性，让我有了新的思路。',
            '我在实践中遇到了类似的问题，按照文章的方法解决了，谢谢分享！',
            '期待更多这样的优质内容！',
            '文章逻辑清晰，案例实用，点赞！',
            '我是初学者，这篇文章对我帮助很大。',
            '我分享到了我的团队群，大家都说很有用，感谢作者！'
        ];
        
        for (let i = 0; i < commentCount; i++) {
            const user = users[Math.floor(Math.random() * users.length)];
            const content = commentContents[Math.floor(Math.random() * commentContents.length)];
            
            const now = new Date();
            const randomTime = new Date(now.getTime() - Math.floor(Math.random() * 10) * 24 * 60 * 60 * 1000);
            
            comments.push({
                id: i + 1,
                username: user.username,
                avatar: user.avatar,
                content: content,
                date: randomTime.toISOString(),
                likes: Math.floor(Math.random() * 20)
            });
        }
        
        return comments;
    }
    
    // 生成模拟相关文章
    function generateMockRelatedArticles() {
        const articleCount = 5;
        const articles = [];
        
        for (let i = 1; i <= articleCount; i++) {
            articles.push({
                id: Math.floor(Math.random() * 100) + 1,
                title: `相关文章 ${i}: 这是一个相关的博客文章标题`,
                thumbnail: `https://picsum.photos/300/200?random=${Math.random()}`,
                views: Math.floor(Math.random() * 500),
                likes: Math.floor(Math.random() * 100)
            });
        }
        
        return articles;
    }

    // 添加一个专门处理翻页点击的函数
    function handleArticleNavigation(articleId) {
        // 保存当前滚动位置
        const scrollPosition = window.scrollY;
        
        // 添加加载指示器
        const postContainer = document.getElementById('postContainer');
        postContainer.classList.add('loading');
        
        // 在URL中设置新ID但不刷新页面
        window.history.pushState({}, '', `blog_detail.html?id=${articleId}`);
        
        // 加载新文章
        fetch(`/api/article/${articleId}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                if (data.article) {
                    // 更新全局文章ID
                    articleId = data.article.id;
                    
                    // 渲染新文章
                    renderArticle(data.article);
                    
                    // 加载评论
                    loadComments(articleId);
                    
                    // 加载相关文章
                    loadRelatedArticles(data.article.category, articleId);
                    
                    // 生成目录
                    generateTableOfContents();
                    
                    // 检查用户是否已点赞或收藏
                    checkUserInteractions();
                    
                    // 显示通知
                    showNotification('文章加载成功', 'success');
                    
                    // 滚动到页面顶部
                    window.scrollTo({
                        top: 0,
                        behavior: 'smooth'
                    });
                } else {
                    throw new Error('文章不存在');
                }
            })
            .catch(error => {
                console.error('加载文章失败:', error);
                showNotification('加载文章失败，请重试', 'error');
                
                // 恢复之前的URL
                window.history.replaceState({}, '', `blog_detail.html?id=${articleId}`);
                
                // 恢复滚动位置
                window.scrollTo(0, scrollPosition);
            })
            .finally(() => {
                postContainer.classList.remove('loading');
            });
    }
});