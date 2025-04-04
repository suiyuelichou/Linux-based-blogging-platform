document.addEventListener('DOMContentLoaded', function() {
    let articleId = null;
    let isLiked = false;
    let isBookmarked = false;
    // 确保必要的库已加载
    loadResources().then(() => {
        // 全局变量
        articleId = null;
        isLiked = false;
        isBookmarked = false;
        
        // 初始化
        init();
    });
    
    // 加载必要的资源
    function loadResources() {
        return Promise.all([
            // 确保 Marked.js 加载
            loadScriptIfNeeded('https://cdn.jsdelivr.net/npm/marked/marked.min.js'),
            // 确保 Highlight.js 加载
            loadScriptIfNeeded('https://cdn.jsdelivr.net/npm/highlight.js@11.7.0/lib/highlight.min.js')
        ]).then(() => {
            // 加载代码高亮的 CSS
            if (window.hljs) {
                loadCSS('https://cdn.jsdelivr.net/npm/highlight.js@11.7.0/styles/github.min.css');
            }
        });
    }
    
    // 按需加载脚本
    function loadScriptIfNeeded(url) {
        return new Promise((resolve) => {
            // 检查脚本是否已加载
            if (url.includes('marked') && window.marked) {
                resolve();
                return;
            }
            if (url.includes('highlight') && window.hljs) {
                resolve();
                return;
            }
            
            const script = document.createElement('script');
            script.src = url;
            script.onload = resolve;
            script.onerror = resolve; // 即使加载失败也继续
            document.head.appendChild(script);
        });
    }
    
    // 加载 CSS
    function loadCSS(url) {
        // 检查是否已加载
        const links = document.querySelectorAll('link');
        for (let i = 0; i < links.length; i++) {
            if (links[i].href.includes(url)) {
                return;
            }
        }
        
        const link = document.createElement('link');
        link.rel = 'stylesheet';
        link.href = url;
        document.head.appendChild(link);
    }
    
    // 初始化
    function init() {
        setupEventListeners();
        loadArticleDetails();
        checkAuthStatus();
        setupDarkMode();
        reorganizeDOM();
        setupUserData();
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
        
        // 添加目录折叠/展开功能
        const tocTitle = document.querySelector('.toc-title');
        if (tocTitle) {
            tocTitle.addEventListener('click', function() {
                const toc = document.querySelector('.toc');
                toc.classList.toggle('collapsed');
            });
        }
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
        
        // 获取博客容器并创建包装元素
        const postContainer = document.getElementById('postContainer');
        
        // 检查是否已经存在包装元素
        let postWrapper = postContainer.querySelector('.post-wrapper');
        if (!postWrapper) {
            // 创建博客内容包装器
            postWrapper = document.createElement('div');
            postWrapper.className = 'post-wrapper';
            
            // 获取所有博客内容相关元素
            const postElements = Array.from(postContainer.children).filter(el => 
                !el.classList.contains('comments-section') && 
                !el.classList.contains('post-navigation'));
            
            // 将它们移动到包装器中
            postElements.forEach(el => {
                // 克隆元素到新容器
                postWrapper.appendChild(el);
            });
            
            // 添加包装器到容器的开头
            postContainer.prepend(postWrapper);
            
            // 创建评论包装器
            const commentsSection = postContainer.querySelector('.comments-section');
            if (commentsSection) {
                const commentsWrapper = document.createElement('div');
                commentsWrapper.className = 'comments-wrapper';
                commentsSection.parentNode.insertBefore(commentsWrapper, commentsSection);
                commentsWrapper.appendChild(commentsSection);
            }
        }
        
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
        
        // 处理可能的超长标题
        handleLongText(postTitle);
        
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
        
        viewCount.textContent = article.views || 0;
        likeCount.textContent = article.likes || 0;
        commentCount.textContent = article.comments || 0;
        
        // 更新评论区的评论数
        const commentCountDisplay = document.getElementById('commentCountDisplay');
        commentCountDisplay.textContent = article.comments || 0;
        
        // 内容处理
        const postContent = document.getElementById('postContent');
        
        // 判断内容格式并显示
        if (article.content_format === 'markdown' && typeof article.content === 'string') {
            // 如果是Markdown格式，使用marked渲染
            postContent.innerHTML = renderMarkdown(article.content);
        } else if (article.content_html) {
            // 如果有预渲染的HTML，直接使用
            postContent.innerHTML = article.content_html;
        } else {
            // 否则使用原始内容
            postContent.innerHTML = article.content;
        }
        
        // 处理代码块
        processQuillCodeBlocks(postContent);
        
        // 处理列表
        processLists(postContent);
        
        // 处理长文本
        handleLongContentText(postContent);
        
        // 更新标签
        const postTags = document.getElementById('postTags');
        if (article.tags && article.tags.length > 0) {
            // 清空现有标签
            postTags.innerHTML = '';
            
            // 添加新标签
            article.tags.forEach(tag => {
                const tagLink = document.createElement('a');
                tagLink.href = `blog_categories.html?tag=${encodeURIComponent(tag)}`;
                tagLink.className = 'post-tag';
                tagLink.textContent = tag;
                postTags.appendChild(tagLink);
            });
        } else {
            postTags.innerHTML = '<span class="no-tags">暂无标签</span>';
        }
        
        // 更新上一篇/下一篇导航
        const prevPost = document.getElementById('prevPost');
        const nextPost = document.getElementById('nextPost');
        
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
        
        // 生成目录
        generateTableOfContents();
        
        // 检查用户是否是文章作者
        checkIfUserIsArticleAuthor(article.author);
    }
    
    // 检查当前登录用户是否为文章作者
    function checkIfUserIsArticleAuthor(articleAuthor) {
        if (!articleAuthor) return;
        
        // 获取当前登录的用户名
        const userAvatar = document.getElementById('userAvatar');
        const username = userAvatar ? userAvatar.getAttribute('data-username') : null;
        
        if (username && articleAuthor === username) {
            // 当前用户是文章作者，显示编辑按钮
            showEditButton();
        } else {
            // 当前用户不是文章作者，隐藏编辑按钮
            hideEditButton();
        }
    }
    
    // 添加新函数：处理Quill代码块
    function processQuillCodeBlocks(contentElement) {
        // 处理Quill特殊代码块结构
        const quillCodeBlocks = contentElement.querySelectorAll('.ql-code-block-container');
        quillCodeBlocks.forEach(container => {
            // 获取所有代码行
            const codeLines = container.querySelectorAll('.ql-code-block');
            if (!codeLines.length) return;
            
            // 收集所有代码行的文本，使用换行符连接
            const codeText = Array.from(codeLines)
                .map(line => line.textContent)
                .join('\n');
            
            // 创建新的pre和code元素
            const preElement = document.createElement('pre');
            const codeElement = document.createElement('code');
            
            // 设置样式
            preElement.style.backgroundColor = 'var(--code-bg, #f6f8fa)';
            preElement.style.padding = '16px';
            preElement.style.borderRadius = '8px';
            preElement.style.overflowX = 'auto';
            preElement.style.margin = '1.5em 0';
            preElement.style.maxWidth = '100%';
            preElement.style.boxShadow = '0 2px 8px rgba(0,0,0,0.1)';
            preElement.style.border = '1px solid var(--border-color)';
            
            codeElement.style.fontFamily = 'SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace';
            codeElement.style.fontSize = '0.9em';
            codeElement.style.display = 'block';
            codeElement.style.lineHeight = '1.6';
            codeElement.style.whiteSpace = 'pre'; // 保留空格和换行
            codeElement.style.tabSize = '2';
            codeElement.style.color = 'var(--text-primary)';
            
            // 设置代码内容，保留换行
            codeElement.textContent = codeText;
            
            // 组装元素
            preElement.appendChild(codeElement);
            
            // 替换原容器
            container.parentNode.replaceChild(preElement, container);
        });

        // 处理可能存在的其他代码块形式
        const qlSyntaxBlocks = contentElement.querySelectorAll('.ql-syntax');
        qlSyntaxBlocks.forEach(block => {
            // 跳过已处理的元素
            if (block.closest('.processed')) return;
            
            // 获取原始内容
            const codeText = block.textContent;
            
            // 创建新元素
            const preElement = document.createElement('pre');
            const codeElement = document.createElement('code');
            
            // 设置样式
            preElement.style.backgroundColor = 'var(--code-bg, #f6f8fa)';
            preElement.style.padding = '16px';
            preElement.style.borderRadius = '8px';
            preElement.style.overflowX = 'auto';
            preElement.style.margin = '1.5em 0';
            preElement.style.maxWidth = '100%';
            preElement.style.boxShadow = '0 2px 8px rgba(0,0,0,0.1)';
            preElement.style.border = '1px solid var(--border-color)';
            
            codeElement.style.fontFamily = 'SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace';
            codeElement.style.fontSize = '0.9em';
            codeElement.style.display = 'block';
            codeElement.style.lineHeight = '1.6';
            codeElement.style.whiteSpace = 'pre';
            codeElement.style.color = 'var(--text-primary)';
            
            // 设置内容
            codeElement.textContent = codeText;
            
            // 组装元素
            preElement.appendChild(codeElement);
            preElement.classList.add('processed');
            
            // 替换原元素
            if (block.parentNode.tagName === 'PRE') {
                block.parentNode.parentNode.replaceChild(preElement, block.parentNode);
            } else {
                block.parentNode.replaceChild(preElement, block);
            }
        });

        // 处理内联代码
        const inlineCodes = contentElement.querySelectorAll('code:not(pre code)');
        inlineCodes.forEach(code => {
            code.style.backgroundColor = 'var(--code-bg, #f1f1f1)';
            code.style.padding = '3px 6px';
            code.style.borderRadius = '4px';
            code.style.fontFamily = 'SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace';
            code.style.fontSize = '0.9em';
            code.style.color = 'var(--inline-code-color, #e83e8c)';
            code.style.wordBreak = 'break-word';
        });
    }
    
    // 处理Quill编辑器生成的列表
    function processLists(contentElement) {
        if (!contentElement) return;
        
        // 隐藏Quill UI元素
        const qlUiElements = contentElement.querySelectorAll('.ql-ui');
        qlUiElements.forEach(el => {
            el.style.display = 'none';
        });
        
        // 处理列表
        const allLists = contentElement.querySelectorAll('ol, ul');
        allLists.forEach(list => {
            // 检查列表项类型
            const listItems = list.querySelectorAll('li');
            let bulletCount = 0;
            let orderedCount = 0;
            
            // 计算不同类型的列表项数量
            listItems.forEach(item => {
                if (item.getAttribute('data-list') === 'bullet') {
                    bulletCount++;
                } else if (item.getAttribute('data-list') === 'ordered') {
                    orderedCount++;
                }
            });
            
            // 根据主要列表项类型设置列表样式
            if (bulletCount > 0 && orderedCount === 0) {
                // 纯无序列表
                list.classList.add('quill-unordered-list');
                list.style.listStyleType = 'disc';
            } else if (orderedCount > 0 && bulletCount === 0) {
                // 纯有序列表
                list.classList.add('quill-ordered-list');
                list.style.listStyleType = 'decimal';
            }
            
            // 确保每个列表项都有正确的样式
            listItems.forEach(item => {
                const listType = item.getAttribute('data-list');
                
                // 确保列表项正确显示
                item.style.display = 'list-item';
                
                if (listType === 'bullet') {
                    item.style.listStyleType = 'disc';
                } else if (listType === 'ordered') {
                    item.style.listStyleType = 'decimal';
                }
                
                // 确保所有ql-ui元素都被隐藏
                const uiElements = item.querySelectorAll('.ql-ui');
                uiElements.forEach(el => {
                    el.style.display = 'none';
                });
            });
        });
    }
    
    // 渲染 Markdown 内容
    function renderMarkdown(markdown) {
        if (!window.marked) {
            console.error('Marked库未加载，无法渲染Markdown');
            return `<pre>${markdown}</pre>`;
        }
        
        // 配置 Marked
        marked.setOptions({
            gfm: true,                  // 启用 GitHub 风格 Markdown
            breaks: true,               // 允许回车换行
            smartLists: true,           // 智能列表
            smartypants: true,          // 智能标点
            xhtml: false,               // 不使用 xhtml 闭合标签
            highlight: function(code, lang) {
                // 如果指定了语言且 hljs 可用，应用代码高亮
                if (lang && window.hljs) {
                    try {
                        return hljs.highlight(code, {language: lang}).value;
                    } catch (e) {
                        return code;
                    }
                }
                return code;
            }
        });
        
        try {
            return marked.parse(markdown);
        } catch (e) {
            console.error('Markdown 渲染错误:', e);
            return `<pre>${markdown}</pre>`;
        }
    }
    
    // 判断内容是否可能是 Markdown
    function isLikelyMarkdown(content) {
        // 检查常见的 Markdown 语法特征
        const markdownPatterns = [
            /^#+ /m,               // 标题
            /\*\*.*\*\*/,          // 粗体
            /\*.*\*/,              // 斜体
            /\[.*\]\(.*\)/,        // 链接
            /^- /m,                // 无序列表
            /^[0-9]+\. /m,         // 有序列表
            /^```[\s\S]*```$/m,    // 代码块
            /^>/m                  // 引用
        ];
        
        return markdownPatterns.some(pattern => pattern.test(content));
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
        commentsList.innerHTML = '';
        
        if (!comments || comments.length === 0) {
            commentsList.innerHTML = '<div class="no-comments-message">暂无评论，来说点什么吧！</div>';
            return;
        }
        
        // 获取当前登录用户
        const currentUsername = getCurrentUsername();
        
        comments.forEach(comment => {
            const isOwnComment = currentUsername && currentUsername === comment.username;
            
            const commentElement = document.createElement('div');
            commentElement.className = `comment ${isOwnComment ? 'own-comment' : ''}`;
            commentElement.setAttribute('data-comment-id', comment.id);
            
            commentElement.innerHTML = `
                <img src="${comment.avatar || 'img/default_touxiang.jpg'}" alt="${comment.username}" class="comment-avatar">
                <div class="comment-body">
                    <div class="comment-header">
                        <span class="comment-author">${comment.username}</span>
                        <span class="comment-date">${formatDate(comment.date)}</span>
                    </div>
                    <div class="comment-text">${comment.content}</div>
                    ${isOwnComment ? `
                    <div class="comment-actions">
                        <button class="comment-action delete-action" data-comment-id="${comment.id}">
                            <i class="far fa-trash-alt"></i> 删除
                        </button>
                    </div>
                    ` : ''}
                </div>
            `;
            
            commentsList.appendChild(commentElement);
        });
        
        // 更新评论计数显示
        document.getElementById('commentCountDisplay').textContent = comments.length;
        
        // 只添加删除按钮的事件监听
        addCommentEventListeners();
    }
    
    // 获取当前登录用户名
    function getCurrentUsername() {
        // 检查是否已登录，从用户头像或其他元素获取用户名
        const userDropdown = document.querySelector('.user-dropdown');
        const logoutButton = document.getElementById('logout');
        
        if (userDropdown && logoutButton) {
            // 用户已登录，尝试获取用户名
            const userAvatar = document.getElementById('userAvatar');
            // 假设用户头像上有 data-username 属性，或者从其他地方获取
            return userAvatar.getAttribute('data-username') || localStorage.getItem('username');
        }
        
        return null; // 未登录
    }
    
    // 添加评论的事件监听器
    function addCommentEventListeners() {
        // 仅添加删除按钮事件监听
        document.querySelectorAll('.delete-action').forEach(button => {
            button.addEventListener('click', function() {
                const commentId = this.getAttribute('data-comment-id');
                showDeleteConfirmation(commentId);
            });
        });
    }
    
    // 显示删除确认对话框
    function showDeleteConfirmation(commentId) {
        // 创建确认对话框
        const overlay = document.createElement('div');
        overlay.className = 'delete-confirmation-overlay';
        
        overlay.innerHTML = `
            <div class="delete-confirmation-dialog">
                <h4>删除评论</h4>
                <p>确定要删除这条评论吗？此操作无法撤销。</p>
                <div class="delete-confirmation-actions">
                    <button class="cancel-delete">取消</button>
                    <button class="confirm-delete">确认删除</button>
                </div>
            </div>
        `;
        
        document.body.appendChild(overlay);
        
        // 绑定事件
        overlay.querySelector('.cancel-delete').addEventListener('click', () => {
            document.body.removeChild(overlay);
        });
        
        overlay.querySelector('.confirm-delete').addEventListener('click', () => {
            document.body.removeChild(overlay);
            deleteComment(commentId);
        });
        
        // 点击背景关闭对话框
        overlay.addEventListener('click', (e) => {
            if (e.target === overlay) {
                document.body.removeChild(overlay);
            }
        });
    }
    
    // 删除评论的函数
    function deleteComment(commentId) {
        // 显示加载状态
        showNotification('正在删除评论...', 'info');
        
        // 调用删除评论API
        fetch(`/api/comments/${commentId}`, {
            method: 'DELETE',
            headers: {
                'Content-Type': 'application/json'
            },
            credentials: 'include' // 包含cookie以验证用户身份
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('删除评论失败');
            }
            return response.json();
        })
        .then(data => {
            // 删除成功
            showNotification('评论已成功删除', 'success');
            
            // 从DOM中移除评论
            const commentElement = document.querySelector(`.comment[data-comment-id="${commentId}"]`);
            if (commentElement) {
                commentElement.remove();
                
                // 更新评论计数
                const commentCountDisplay = document.getElementById('commentCountDisplay');
                const newCount = parseInt(commentCountDisplay.textContent) - 1;
                commentCountDisplay.textContent = newCount;
                
                // 检查是否没有评论了
                if (newCount === 0) {
                    document.getElementById('commentsList').innerHTML = 
                        '<div class="no-comments-message">暂无评论，来说点什么吧！</div>';
                }
            }
        })
        .catch(error => {
            console.error('删除评论失败:', error);
            showNotification('删除评论失败: ' + error.message, 'error');
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
            
            // 确保标题不会太长
            const truncatedTitle = post.title.length > 50 ? post.title.substring(0, 50) + '...' : post.title;
            
            html += `
            <li class="popular-post">
                <a href="blog_detail.html?id=${post.id}" class="popular-post-image">
                    <img src="${thumbnail}" alt="${truncatedTitle}">
                </a>
                <div class="popular-post-info">
                    <h4><a href="blog_detail.html?id=${post.id}" title="${post.title}" class="related-article-link">${post.title}</a></h4>
                    <div class="meta">
                        <span><i class="fas fa-eye"></i> ${post.views}</span>
                        <span><i class="fas fa-heart"></i> ${post.likes}</span>
                    </div>
                </div>
            </li>
            `;
        });
        
        relatedPosts.innerHTML = html;
        
        // 处理相关文章中的长标题
        document.querySelectorAll('.popular-post-info h4 a').forEach(titleLink => {
            handleLongText(titleLink);
            
            // 添加title属性，以便鼠标悬停时显示完整标题
            if (!titleLink.hasAttribute('title')) {
                titleLink.setAttribute('title', titleLink.textContent);
            }
            
            // 检查是否需要省略号
            const titleHeight = titleLink.offsetHeight;
            const lineHeight = parseInt(window.getComputedStyle(titleLink).lineHeight);
            const maxLines = 2;
            
            if (titleHeight > lineHeight * maxLines) {
                titleLink.classList.add('truncated');
            }
        });
    }
    
    // 生成文章目录
    function generateTableOfContents() {
        const content = document.getElementById('postContent');
        const tocList = document.getElementById('tocList');
        
        if (!content || !tocList) return;
        
        // 获取所有标题元素
        const headings = content.querySelectorAll('h1, h2, h3, h4, h5, h6');
        
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
        
        // 为目录链接添加点击事件，实现滚动到页面中间的效果
        const tocLinks = tocList.querySelectorAll('a');
        tocLinks.forEach(link => {
            link.addEventListener('click', function(e) {
                e.preventDefault(); // 阻止默认锚点跳转
                
                // 获取目标标题的ID
                const targetId = this.getAttribute('href').substring(1);
                const targetElement = document.getElementById(targetId);
                
                if (targetElement) {
                    // 计算滚动位置：使目标元素位于页面中间
                    const offset = 50; // 顶部导航栏高度的偏移量
                    const targetPosition = targetElement.getBoundingClientRect().top + window.pageYOffset;
                    const offsetPosition = targetPosition - (window.innerHeight / 2) + (targetElement.offsetHeight / 2) - offset;
                    
                    // 平滑滚动到目标位置
                    window.scrollTo({
                        top: offsetPosition,
                        behavior: 'smooth'
                    });
                }
            });
        });
        
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
            const noComments = commentsList.querySelector('.no-comments-message');
            
            if (noComments) {
                commentsList.innerHTML = '';
            }
            
            // 创建新评论元素 - 修改此部分添加删除按钮
            const newComment = document.createElement('div');
            newComment.className = 'comment own-comment'; // 添加own-comment类标识这是当前用户的评论
            newComment.setAttribute('data-comment-id', data.id);
            
            newComment.innerHTML = `
                <img src="${user.avatar || 'img/default_touxiang.jpg'}" alt="${user.username}" class="comment-avatar">
                <div class="comment-body">
                    <div class="comment-header">
                        <span class="comment-author">${user.username}</span>
                        <span class="comment-date">刚刚</span>
                    </div>
                    <div class="comment-text">${commentContent}</div>
                    <div class="comment-actions">
                        <button class="comment-action delete-action" data-comment-id="${data.id}">
                            <i class="far fa-trash-alt"></i> 删除
                        </button>
                    </div>
                </div>
            `;
            
            commentsList.prepend(newComment);
            
            // 为新添加的删除按钮绑定事件监听器
            const deleteButton = newComment.querySelector('.delete-action');
            deleteButton.addEventListener('click', function() {
                const commentId = this.getAttribute('data-comment-id');
                showDeleteConfirmation(commentId);
            });
            
            // 更新评论数量
            const commentCount = document.getElementById('commentCount');
            const commentCountDisplay = document.getElementById('commentCountDisplay');
            const newCount = parseInt(commentCount.textContent) + 1;
            commentCount.textContent = newCount;
            commentCountDisplay.textContent = newCount;
            
            showNotification('评论发表成功', 'success');
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
            if (userAvatar) userAvatar.setAttribute('data-username', userData.username || '');
            
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
            
            // 检查是否为文章作者
            checkIfCurrentUserIsAuthor(userData.username);
        } else {
            // 用户未登录，显示游客信息
            if (userAvatar) userAvatar.src = 'img/default_touxiang.jpg';
            if (userAvatar) userAvatar.removeAttribute('data-username');
            
            // 更新下拉菜单
            if (userDropdown) {
                userDropdown.innerHTML = `
                <a href="blog_login.html"><i class="fas fa-sign-in-alt"></i> 登录</a>
                <a href="blog_register.html"><i class="fas fa-user-plus"></i> 注册</a>
                `;
            }
            
            // 隐藏编辑按钮
            hideEditButton();
        }
    }
    
    // 检查当前用户是否为文章作者
    function checkIfCurrentUserIsAuthor(username) {
        if (!username) return;
        
        const authorName = document.getElementById('authorName');
        if (authorName && authorName.textContent === username) {
            // 当前用户是文章作者，显示编辑按钮
            showEditButton();
        } else {
            // 当前用户不是文章作者，隐藏编辑按钮
            hideEditButton();
        }
    }
    
    // 显示编辑按钮
    function showEditButton() {
        const editBtn = document.getElementById('editBtn');
        if (editBtn) {
            editBtn.style.display = 'inline-flex';
            
            // 添加点击事件
            editBtn.addEventListener('click', function() {
                const urlParams = new URLSearchParams(window.location.search);
                const articleId = urlParams.get('id');
                if (articleId) {
                    // 执行与用户中心相同的编辑逻辑
                    editBlog(articleId);
                } else {
                    showNotification('无法获取文章ID，请刷新页面后重试', 'error');
                }
            });
        }
    }
    
    // 隐藏编辑按钮
    function hideEditButton() {
        const editBtn = document.getElementById('editBtn');
        if (editBtn) {
            editBtn.style.display = 'none';
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
## 引言

欢迎阅读这篇博客文章！这是一篇模拟生成的文章，用于展示博客详情页面的设计和功能。

在这篇文章中，我们将探讨一些关于Web开发的重要概念和最佳实践。

## 第一部分：基础知识

在Web开发中，HTML、CSS和JavaScript构成了基础的三大技术栈。

### HTML的重要性

HTML（超文本标记语言）是构建网页的基础。它提供了网页的基本结构和内容。

一个良好的HTML结构应该具备以下特点：

- 语义化：使用合适的标签表达内容的含义
- 可访问性：考虑到所有用户的使用需求
- 良好的SEO：有利于搜索引擎索引

### CSS的作用

CSS（层叠样式表）用于控制网页的样式和布局。

现代CSS已经非常强大，可以实现复杂的布局和动画效果：

- Flexbox和Grid布局
- CSS变量
- CSS动画和过渡

### JavaScript的能力

JavaScript是一种功能强大的编程语言，使网页具有交互性和动态性。

现代JavaScript生态系统包括：

- ES6+的新特性
- 框架和库（React、Vue、Angular等）
- Node.js服务器端JavaScript

## 第二部分：进阶技巧

掌握了基础知识后，让我们来看一些进阶的Web开发技巧。

### 响应式设计

响应式设计是现代Web开发的标准实践，确保网站在不同设备上都能提供良好的用户体验。

\`\`\`css
/* 媒体查询示例 */
@media (max-width: 768px) {
.container {
flex-direction: column;
}
}
\`\`\`

### 性能优化

Web性能对用户体验至关重要。以下是一些关键的性能优化技巧：

- 图片懒加载
- 代码分割
- 缓存策略
- 服务器端渲染

> "性能不是一个功能，而是每个功能的基础要求。"

## 总结

Web开发是一个不断发展的领域，需要持续学习和实践。希望这篇文章能给你带来一些有用的信息和启发。

感谢阅读！如有问题或建议，欢迎在下方评论区留言。
`;
        
        // HTML 版本，用于备用
        const htmlContent = window.marked ? marked.parse(content) : 
            `<h2>引言</h2>
            <p>欢迎阅读这篇博客文章！这是一篇模拟生成的文章，用于展示博客详情页面的设计和功能。</p>
            <p>在这篇文章中，我们将探讨一些关于Web开发的重要概念和最佳实践。</p>
            <!-- 其他 HTML 内容 -->`;
        
        return {
            id: id,
            title: `博客文章标题 ${id}: 这是一个关于Web开发的深入探讨`,
            excerpt: '这是文章的摘要内容，通常显示文章的前几段文字。这只是一个示例文本，用于展示文章卡片的样式和布局效果。',
            content: content,
            content_html: htmlContent, 
            content_format: 'markdown',
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
            const id = Math.floor(Math.random() * 100) + 1;
            articles.push({
                id: id,
                title: `相关文章 ${i}: 这是一个相关的博客文章标题`,
                thumbnail: `https://picsum.photos/300/200?random=${id}`,
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

    // 添加新的函数：处理长文本，为超长单词添加特殊处理
    function handleLongText(element) {
        if (!element) return;
        
        const text = element.textContent;
        
        // 检查总长度
        if (text.length > 50) {
            element.classList.add('long-word-container');
        }
        
        // 检查是否有超长单词（超过20个字符）
        const words = text.split(/\s+/);
        const hasLongWord = words.some(word => word.length > 20);
        
        if (hasLongWord) {
            // 为元素添加特殊类
            element.classList.add('has-long-word');
            
            // 处理超长单词，为它们包裹特殊的span
            const newContent = words.map(word => {
                if (word.length > 20) {
                    return `<span class="long-word">${word}</span>`;
                }
                return word;
            }).join(' ');
            
            element.innerHTML = newContent;
        }
        
        // 检查中文内容是否过长（无空格的长内容）
        if (text.length > 30 && /[\u4e00-\u9fa5]/.test(text) && words.length <= 5) {
            // 为中文长文本添加特殊处理
            element.classList.add('long-word-container');
            element.classList.add('cn-text');
            
            // 处理过长的中文内容，每隔30个字符截断一次
            if (text.length > 60 && element.tagName.toLowerCase() === 'h1') {
                const segments = [];
                for (let i = 0; i < text.length; i += 30) {
                    segments.push(text.substring(i, i + 30));
                }
                element.innerHTML = segments.map(seg => `<span class="cn-segment">${seg}</span>`).join('');
            }
        }
    }
    
    // 处理文章内容中的长文本
    function handleLongContentText(contentElement) {
        if (!contentElement) return;
        
        // 添加容器标记
        contentElement.classList.add('content-processed');
        
        // 处理段落中的长单词和中文
        const textElements = contentElement.querySelectorAll('p, h1, h2, h3, h4, h5, h6, li, a, td, th');
        textElements.forEach(el => {
            const text = el.textContent;
            const words = text.split(/\s+/);
            
            // 检查是否有超长单词
            const hasLongWord = words.some(word => word.length > 30);
            
            if (hasLongWord) {
                // 标记包含长单词的元素
                el.classList.add('has-long-word');
                
                // 处理超长单词
                if (el.tagName.toLowerCase() !== 'a') { // 不处理链接内部
                    // 替换超长单词为带特殊类的span
                    const newContent = words.map(word => {
                        if (word.length > 30) {
                            return `<span class="long-word">${word}</span>`;
                        }
                        return word;
                    }).join(' ');
                    
                    // 仅当元素没有内嵌元素时才更新innerHTML
                    if (!el.querySelector('*')) {
                        el.innerHTML = newContent;
                    }
                }
            }
            
            // 处理中文长句
            if (text.length > 50 && /[\u4e00-\u9fa5]/.test(text) && words.length <= 5) {
                el.classList.add('cn-text');
                el.classList.add('long-word-container');
            }
        });
        
        // 处理代码块，确保长代码能够换行
        const codeBlocks = contentElement.querySelectorAll('pre code');
        codeBlocks.forEach(code => {
            code.classList.add('wrap-code');
        });
        
        // 处理表格，确保能够滚动
        const tables = contentElement.querySelectorAll('table');
        tables.forEach(table => {
            // 检查表格宽度
            if (table.offsetWidth > contentElement.offsetWidth) {
                table.classList.add('wide-table');
            }
            
            // 如果表格不在div中，则包裹一个div
            if (table.parentElement.tagName !== 'DIV' || !table.parentElement.classList.contains('table-wrapper')) {
                const wrapper = document.createElement('div');
                wrapper.className = 'table-wrapper';
                table.parentNode.insertBefore(wrapper, table);
                wrapper.appendChild(table);
            }
        });
        
        // 处理图片
        const images = contentElement.querySelectorAll('img');
        images.forEach(img => {
            img.classList.add('responsive-img');
            img.setAttribute('loading', 'lazy');
            
            // 确保图片不会超出容器
            img.style.maxWidth = '100%';
            img.style.height = 'auto';
            
            // 检查图片是否有父容器
            if (img.parentElement.tagName !== 'DIV' || !img.parentElement.classList.contains('img-wrapper')) {
                const wrapper = document.createElement('div');
                wrapper.className = 'img-wrapper';
                img.parentNode.insertBefore(wrapper, img);
                wrapper.appendChild(img);
            }
        });
        
        // 处理长链接
        const links = contentElement.querySelectorAll('a');
        links.forEach(link => {
            link.classList.add('processed-link');
            
            if (link.textContent.length > 30) {
                link.classList.add('long-link');
            }
            
            // 如果链接文本是链接本身，并且很长
            if (link.textContent === link.href && link.textContent.length > 30) {
                link.classList.add('url-link');
                // 截断显示的URL (保留前15个字符和后15个字符)
                const url = link.textContent;
                link.setAttribute('title', url);
                link.innerHTML = `${url.substring(0, 15)}...${url.substring(url.length - 15)}`;
            }
        });
    }

    // 添加一个新函数以重新组织DOM结构
    function reorganizeDOM() {
        // 在页面加载后设置超时，确保DOM已完全加载
        setTimeout(() => {
            const postContainer = document.getElementById('postContainer');
            if (!postContainer) return;
            
            // 检查是否已经存在包装元素
            if (postContainer.querySelector('.post-wrapper')) return;
            
            // 创建博客内容包装器
            const postWrapper = document.createElement('div');
            postWrapper.className = 'post-wrapper';
            
            // 获取博客头部和内容
            const postHeader = postContainer.querySelector('.post-header');
            const postContent = postContainer.querySelector('.post-content');
            const postTags = postContainer.querySelector('.post-tags');
            
            if (postHeader) postWrapper.appendChild(postHeader.cloneNode(true));
            if (postContent) postWrapper.appendChild(postContent.cloneNode(true));
            if (postTags) postWrapper.appendChild(postTags.cloneNode(true));
            
            // 删除原始元素
            if (postHeader) postHeader.remove();
            if (postContent) postContent.remove();
            if (postTags) postTags.remove();
            
            // 添加包装器到容器的开头
            postContainer.prepend(postWrapper);
            
            // 创建评论包装器
            const commentsSection = postContainer.querySelector('.comments-section');
            if (commentsSection) {
                const commentsWrapper = document.createElement('div');
                commentsWrapper.className = 'comments-wrapper';
                commentsSection.parentNode.insertBefore(commentsWrapper, commentsSection);
                commentsWrapper.appendChild(commentsSection);
            }
            
            // 修复后备方案中的列表样式问题
            const ol = document.querySelectorAll('.post-content ol');
            if (ol.length > 0) {
                ol.forEach(list => {
                    list.style.counterReset = 'list-counter';
                });
            }
        }, 500);
    }

    // 在页面加载时，确保设置用户名数据
    function setupUserData() {
        // 检查用户是否已登录
        const userDropdown = document.querySelector('.user-dropdown');
        const logoutButton = document.getElementById('logout');
        
        if (userDropdown && logoutButton) {
            // 用户已登录，获取用户信息
            fetch('/api/user/info', {
                method: 'GET',
                credentials: 'include'
            })
            .then(response => response.json())
            .then(data => {
                if (data.username) {
                    // 将用户名存储在头像元素的data属性中
                    const userAvatar = document.getElementById('userAvatar');
                    userAvatar.setAttribute('data-username', data.username);
                    // 也可以存储在localStorage中作为备份
                    localStorage.setItem('username', data.username);
                }
            })
            .catch(error => {
                console.error('获取用户信息失败:', error);
            });
        }
    }

    // 编辑博客（复用用户中心的逻辑）
    async function editBlog(blogId) {
        if (!blogId) {
            showNotification('无效的博客ID', 'error');
            return;
        }
        
        try {
            // 显示加载状态
            showNotification('正在加载博客数据...', 'info');
            
            // 获取博客详情
            const response = await fetch(`/api/blogs/${blogId}`);
            
            if (!response.ok) {
                throw new Error('获取博客数据失败');
            }
            
            let blogData = await response.json();
            
            // 处理不同格式的API响应
            if (blogData.code && blogData.data) {
                blogData = blogData.data;
            }
            
            // 确保统一封面图片字段名
            if (!blogData.coverImage) {
                // 尝试其他可能的字段名
                blogData.coverImage = blogData.cover_image || blogData.thumbnail || blogData.cover || '';
            }
            
            // 确保标签字段格式正确
            if (typeof blogData.tags === 'string') {
                try {
                    // 尝试解析JSON字符串
                    const parsedTags = JSON.parse(blogData.tags);
                    if (Array.isArray(parsedTags)) {
                        blogData.tags = parsedTags;
                    } else {
                        // 如果解析结果不是数组，假设是逗号分隔的字符串
                        blogData.tags = blogData.tags.split(',').map(tag => tag.trim()).filter(tag => tag);
                    }
                } catch (e) {
                    // 如果解析失败，假设是逗号分隔的字符串
                    blogData.tags = blogData.tags.split(',').map(tag => tag.trim()).filter(tag => tag);
                }
            } else if (!Array.isArray(blogData.tags)) {
                // 如果不是字符串也不是数组，设为空数组
                blogData.tags = [];
            }
            
            // 将博客数据存储到sessionStorage
            sessionStorage.setItem('editBlogData', JSON.stringify(blogData));
            
            // 跳转到博客编辑页面
            window.location.href = `blog_editor.html?mode=edit&id=${blogId}`;
        } catch (error) {
            console.error('加载博客数据失败:', error);
            showNotification('加载博客数据失败，请重试', 'error');
        }
    }
});