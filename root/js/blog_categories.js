document.addEventListener('DOMContentLoaded', function() {
    // 全局变量
    let currentPage = 1;
    let totalPages = 1;
    let articlesPerPage = 6;
    let articles = [];
    let categories = [];
    let currentCategory = '';
    let currentView = 'card'; // 'card' 或 'list'
    
    // 初始化
    init();
    
    function init() {
        setupEventListeners();
        loadPopularPosts();
        loadTags();
        checkAuthStatus();
        setupDarkMode();
        
        // 先加载分类列表，因为我们需要在加载文章之前确保分类信息已经准备好
        loadCategories().then(() => {
            // 检查URL参数中是否有指定分类或标签
            const urlParams = new URLSearchParams(window.location.search);
            const categoryParam = urlParams.get('category');
            const tagParam = urlParams.get('tag');
            
            if (tagParam) {
                // 如果有标签参数，优先加载标签相关文章
                loadArticlesByTag(decodeURIComponent(tagParam));
            } else if (categoryParam) {
                // 处理分类参数
                currentCategory = decodeURIComponent(categoryParam);
                // 更新分类卡片的选中状态
                updateCategorySelection(currentCategory);
                loadArticlesByCategory(currentCategory);
            } else {
                // 默认加载所有文章
                currentCategory = '';
                updateCategorySelection('');
                loadArticles();
            }
        });
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
            let timeoutId;
            
            // 点击头像显示下拉菜单
            userAvatar.addEventListener('click', function(e) {
                e.stopPropagation();
                userDropdown.classList.toggle('show');
            });
            
            // 鼠标离开头像时不立即隐藏，而是设置延时
            userAvatar.addEventListener('mouseout', function() {
                timeoutId = setTimeout(() => {
                    userDropdown.classList.remove('show');
                }, 300); // 300毫秒延迟
            });
            
            // 鼠标进入下拉菜单时，取消隐藏计时器
            userDropdown.addEventListener('mouseover', function() {
                clearTimeout(timeoutId);
            });
            
            // 鼠标离开下拉菜单时，设置延时隐藏
            userDropdown.addEventListener('mouseout', function() {
                timeoutId = setTimeout(() => {
                    userDropdown.classList.remove('show');
                }, 300); // 300毫秒延迟
            });
            
            // 点击页面其他区域关闭菜单
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
        
        // 视图切换
        const viewButtons = document.querySelectorAll('.view-options button');
        viewButtons.forEach(button => {
            button.addEventListener('click', function() {
                const view = this.getAttribute('data-view');
                changeView(view);
                
                // 更新活动按钮
                viewButtons.forEach(btn => btn.classList.remove('active'));
                this.classList.add('active');
            });
        });
        
        // 分页按钮
        const prevPageBtn = document.getElementById('prevPage');
        const nextPageBtn = document.getElementById('nextPage');
        
        if (prevPageBtn && nextPageBtn) {
            prevPageBtn.addEventListener('click', function() {
                if (currentPage > 1) {
                    loadPage(currentPage - 1);
                }
            });
            
            nextPageBtn.addEventListener('click', function() {
                if (currentPage < totalPages) {
                    loadPage(currentPage + 1);
                }
            });
        }
        
        // 分类搜索
        const categorySearch = document.getElementById('categorySearch');
        if (categorySearch) {
            categorySearch.addEventListener('input', function() {
                filterCategories(this.value);
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
    }
    
    // 加载分类
    function loadCategories() {
        showCategoriesLoading();
        
        return new Promise((resolve, reject) => {
            fetch('/api/categories')
                .then(response => {
                    if (!response.ok) {
                        throw new Error('网络响应异常');
                    }
                    return response.json();
                })
                .then(data => {
                    categories = data.categories || [];
                    
                    // 更新分类统计
                    const totalCategoriesEl = document.getElementById('totalCategories');
                    if (totalCategoriesEl) {
                        totalCategoriesEl.textContent = categories.length;
                    }
                    
                    // 计算所有文章数量总和
                    const totalArticles = categories.reduce((sum, category) => {
                        return sum + parseInt(category.count || 0);
                    }, 0);
                    
                    const totalArticlesEl = document.getElementById('totalArticles');
                    if (totalArticlesEl) {
                        totalArticlesEl.textContent = totalArticles;
                    }
                    
                    renderCategories();
                    resolve(); // 成功时解析 Promise
                })
                .catch(error => {
                    console.error('获取分类列表失败:', error);
                    // 使用模拟数据作为后备方案
                    categories = generateMockCategories();
                    
                    // 更新分类统计
                    const totalCategoriesEl = document.getElementById('totalCategories');
                    if (totalCategoriesEl) {
                        totalCategoriesEl.textContent = categories.length;
                    }
                    
                    // 计算所有文章数量总和
                    const totalArticles = categories.reduce((sum, category) => {
                        return sum + parseInt(category.count || 0);
                    }, 0);
                    
                    const totalArticlesEl = document.getElementById('totalArticles');
                    if (totalArticlesEl) {
                        totalArticlesEl.textContent = totalArticles;
                    }
                    
                    renderCategories();
                    showNotification('获取分类列表失败，显示模拟数据', 'error');
                    resolve(); // 即使使用模拟数据也要解析 Promise
                });
        });
    }
    
    // 渲染分类
    function renderCategories() {
        const categoriesContainer = document.getElementById('categoriesContainer');
        if (!categoriesContainer) return;
        
        if (categories.length === 0) {
            categoriesContainer.innerHTML = `
                <div class="no-categories">
                    <i class="fas fa-folder-open"></i>
                    <p>暂无分类数据</p>
                </div>
            `;
            return;
        }
        
        let html = '';
        
        // 添加"全部分类"选项
        html += `
        <div class="category-card ${currentCategory === '' ? 'active' : ''}" data-category="">
            <i class="fas fa-layer-group category-icon"></i>
            <div class="category-name">全部分类</div>
            <div class="category-count">所有文章</div>
        </div>
        `;
        
        // 渲染各个分类卡片
        categories.forEach((category, index) => {
            // 根据分类名选择合适的图标
            let iconClass = 'fas fa-folder';
            
            if (category.name.includes('技术') || category.name.includes('编程')) {
                iconClass = 'fas fa-code';
            } else if (category.name.includes('生活')) {
                iconClass = 'fas fa-coffee';
            } else if (category.name.includes('旅行')) {
                iconClass = 'fas fa-plane';
            } else if (category.name.includes('美食')) {
                iconClass = 'fas fa-utensils';
            } else if (category.name.includes('设计')) {
                iconClass = 'fas fa-palette';
            } else if (category.name.includes('数据库')) {
                iconClass = 'fas fa-database';
            } else if (category.name.includes('前端')) {
                iconClass = 'fas fa-desktop';
            } else if (category.name.includes('后端')) {
                iconClass = 'fas fa-server';
            } else if (category.name.includes('算法')) {
                iconClass = 'fas fa-sitemap';
            }
            
            html += `
            <div class="category-card ${category.name === currentCategory ? 'active' : ''}" 
                 data-category="${category.name}" 
                 style="animation-delay: ${index * 0.05}s">
                <i class="${iconClass} category-icon"></i>
                <div class="category-name">${category.name}</div>
                <div class="category-count">${category.count} 篇文章</div>
            </div>
            `;
        });
        
        categoriesContainer.innerHTML = html;
        
        // 修改分类卡片的点击事件处理
        const categoryCards = categoriesContainer.querySelectorAll('.category-card');
        categoryCards.forEach(card => {
            card.addEventListener('click', function() {
                const category = this.getAttribute('data-category');
                
                // 更新当前分类
                currentCategory = category;
                
                // 更新URL但不重新加载页面
                const url = new URL(window.location);
                if (category) {
                    url.searchParams.set('category', category);
                    // 确保删除可能存在的tag参数
                    url.searchParams.delete('tag');
                } else {
                    url.searchParams.delete('category');
                    url.searchParams.delete('tag');
                }
                window.history.pushState({}, '', url);
                
                // 更新分类选中状态
                updateCategorySelection(category);
                
                // 重置分页
                currentPage = 1;
                
                // 加载该分类的文章
                if (category) {
                    loadArticlesByCategory(category);
                } else {
                    loadArticles();
                }
                
                // 滚动到文章列表
                const categoryArticles = document.getElementById('categoryArticles');
                if (categoryArticles) {
                    categoryArticles.scrollIntoView({ behavior: 'smooth' });
                }
            });
        });
    }
    
    // 筛选分类
    function filterCategories(searchTerm) {
        if (!searchTerm) {
            // 如果搜索词为空，显示所有分类
            renderCategories();
            return;
        }
        
        searchTerm = searchTerm.toLowerCase();
        
        const categoriesContainer = document.getElementById('categoriesContainer');
        if (!categoriesContainer) return;
        
        const categoryCards = categoriesContainer.querySelectorAll('.category-card');
        
        categoryCards.forEach(card => {
            const categoryName = card.getAttribute('data-category').toLowerCase();
            
            if (categoryName === '' || categoryName.includes(searchTerm)) {
                card.style.display = '';
            } else {
                card.style.display = 'none';
            }
        });
    }
    
    // 加载所有文章
    function loadArticles() {
        showLoading();
        
        // 发送请求到后端获取文章列表
        fetch('/api/articles?page=' + currentPage + '&size=' + articlesPerPage)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                articles = data.articles || [];
                totalPages = data.totalPages || 1;
                
                renderArticles();
                renderPagination();
                hideLoading();
            })
            .catch(error => {
                console.error('获取文章列表失败:', error);
                // 使用模拟数据作为后备方案
                articles = generateMockArticles(20);
                totalPages = Math.ceil(articles.length / articlesPerPage);
                
                // 当使用模拟数据时，我们需要手动分页
                const startIdx = (currentPage - 1) * articlesPerPage;
                const endIdx = Math.min(startIdx + articlesPerPage, articles.length);
                articles = articles.slice(startIdx, endIdx);
                
                renderArticles();
                renderPagination();
                hideLoading();
                
                showNotification('获取文章列表失败，显示模拟数据', 'error');
            });
    }
    
    // 加载特定分类的文章
    function loadArticlesByCategory(category) {
        showLoading();
        
        // 发送请求到后端获取特定分类的文章
        fetch(`/api/articles?category=${encodeURIComponent(category)}&page=${currentPage}&size=${articlesPerPage}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                articles = data.articles || [];
                totalPages = data.totalPages || 1;
                
                renderArticles();
                renderPagination();
                hideLoading();
            })
            .catch(error => {
                console.error(`获取分类 "${category}" 的文章失败:`, error);
                // 使用模拟数据作为后备方案
                let allMockArticles = generateMockArticles(30);
                
                // 过滤出特定分类的文章
                allMockArticles = allMockArticles.filter(article => article.category === category);
                
                totalPages = Math.ceil(allMockArticles.length / articlesPerPage);
                
                // 手动分页
                const startIdx = (currentPage - 1) * articlesPerPage;
                const endIdx = Math.min(startIdx + articlesPerPage, allMockArticles.length);
                articles = allMockArticles.slice(startIdx, endIdx);
                
                renderArticles();
                renderPagination();
                hideLoading();
                
                showNotification(`获取分类 "${category}" 的文章失败，显示模拟数据`, 'error');
            });
    }
    
    // 渲染文章
    function renderArticles() {
        const articlesContainer = document.getElementById('articlesContainer');
        if (!articlesContainer) return;
        
        // 切换视图类
        articlesContainer.className = 'articles-container ' + 
            (currentView === 'list' ? 'list-view' : '');
        
        let html = '';
        
        if (articles.length === 0) {
            html = `
            <div class="no-articles">
                <i class="far fa-file-alt"></i>
                <p>没有找到文章</p>
                <span>当前分类下暂无内容</span>
            </div>`;
        } else {
            articles.forEach((article, index) => {
                // 检查缩略图是否为空，如果为空则使用随机图片
                const thumbnail = article.thumbnail ? article.thumbnail : `https://picsum.photos/600/400?random=${article.id || index}`;
                
                // 处理文章摘要内容，移除HTML标签并限制长度
                const excerpt = extractExcerpt(article.excerpt, 150);
                
                html += `
                <article class="article-card fade-in" style="animation-delay: ${index * 0.1}s">
                    <div class="thumbnail">
                        <img src="${thumbnail}" alt="${article.title}">
                    </div>
                    <div class="content">
                        <span class="category">${article.category}</span>
                        <h3><a href="blog_detail.html?id=${article.id}">${article.title}</a></h3>
                        <p>${excerpt}</p>
                        <div class="article-meta">
                            <div class="author">
                                <img src="${article.authorAvatar}" alt="${article.author}">
                                <span>${article.author}</span>
                            </div>
                            <div class="article-stats">
                                <span><i class="fas fa-eye"></i> ${article.views}</span>
                                <span><i class="fas fa-heart"></i> ${article.likes}</span>
                                <span><i class="fas fa-comment"></i> ${article.comments}</span>
                            </div>
                        </div>
                    </div>
                </article>
                `;
            });
        }
        
        articlesContainer.innerHTML = html;
    }
    
    // 辅助函数：提取HTML内容中的纯文本并生成摘要
    function extractExcerpt(htmlContent, maxLength = 150) {
        if (!htmlContent) return '无内容摘要';
        
        // 创建临时DOM元素来解析HTML
        const tempDiv = document.createElement('div');
        tempDiv.innerHTML = htmlContent;
        
        // 获取纯文本内容
        let textContent = tempDiv.textContent || tempDiv.innerText || '';
        
        // 去除多余空白字符
        textContent = textContent.replace(/\s+/g, ' ').trim();
        
        // 限制长度并添加省略号
        if (textContent.length > maxLength) {
            return textContent.substring(0, maxLength) + '...';
        }
        
        return textContent;
    }
    
    // 渲染分页
    function renderPagination() {
        const pageNumbers = document.getElementById('pageNumbers');
        if (!pageNumbers) return;
        
        let html = '';
        
        // 决定显示哪些页码
        let startPage = Math.max(1, currentPage - 2);
        let endPage = Math.min(totalPages, currentPage + 2);
        
        // 确保始终显示5个页码按钮（如果有足够的页数）
        if (endPage - startPage < 4 && totalPages > 5) {
            if (currentPage < totalPages / 2) {
                endPage = Math.min(startPage + 4, totalPages);
            } else {
                startPage = Math.max(endPage - 4, 1);
            }
        }
        
        // 添加"首页"按钮
        if (startPage > 1) {
            html += `<button onclick="loadPage(1)">1</button>`;
            if (startPage > 2) {
                html += `<button disabled>...</button>`;
            }
        }
        
        // 添加页码按钮
        for (let i = startPage; i <= endPage; i++) {
            html += `<button onclick="loadPage(${i})" ${i === currentPage ? 'class="active"' : ''}>${i}</button>`;
        }
        
        // 添加"末页"按钮
        if (endPage < totalPages) {
            if (endPage < totalPages - 1) {
                html += `<button disabled>...</button>`;
            }
            html += `<button onclick="loadPage(${totalPages})">${totalPages}</button>`;
        }
        
        pageNumbers.innerHTML = html;
        
        // 更新上一页/下一页按钮状态
        const prevPageBtn = document.getElementById('prevPage');
        const nextPageBtn = document.getElementById('nextPage');
        
        if (prevPageBtn) {
            prevPageBtn.disabled = currentPage === 1;
        }
        
        if (nextPageBtn) {
            nextPageBtn.disabled = currentPage === totalPages;
        }
        
        // 全局函数，用于页码点击
        window.loadPage = function(page) {
            if (page < 1 || page > totalPages || page === currentPage) return;
            
            currentPage = page;
            
            // 根据当前是否有选中的分类来加载相应的文章
            if (currentCategory) {
                loadArticlesByCategory(currentCategory);
            } else {
                loadArticles();
            }
            
            // 滚动到内容顶部
            const categoryArticles = document.getElementById('categoryArticles');
            if (categoryArticles) {
                categoryArticles.scrollIntoView({ behavior: 'smooth' });
            }
        };
    }
    
    // 加载热门文章
    function loadPopularPosts() {
        const popularPosts = document.getElementById('popularPosts');
        if (!popularPosts) return;
        
        // 发送请求到后端获取热门文章
        fetch('/api/popular?size=5')
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                const posts = data.articles || [];
                
                let html = '';
                posts.forEach((post, index) => {
                    // 检查缩略图是否为空，如果为空则使用随机图片
                    const thumbnail = post.thumbnail ? post.thumbnail : `https://picsum.photos/600/400?random=${post.id || index + 100}`;
                    
                    // 确保标题是纯文本
                    const title = post.title ? extractExcerpt(post.title, 60) : '无标题';
                    
                    html += `
                    <li class="popular-post">
                        <img src="${thumbnail}" alt="${title}">
                        <div class="popular-post-info">
                            <h4><a href="blog_detail.html?id=${post.id}">${title}</a></h4>
                            <div class="meta">
                                <span><i class="fas fa-eye"></i> ${post.views}</span>
                                <span><i class="fas fa-heart"></i> ${post.likes}</span>
                            </div>
                        </div>
                    </li>
                    `;
                });
                
                popularPosts.innerHTML = html || '<p>暂无热门文章</p>';
            })
            .catch(error => {
                console.error('获取热门文章失败:', error);
                // 使用模拟数据作为后备方案
                const posts = generateMockArticles(5).sort((a, b) => b.views - a.views);
                
                let html = '';
                posts.forEach(post => {
                    html += `
                    <li class="popular-post">
                        <img src="${post.thumbnail}" alt="${post.title}">
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
                
                popularPosts.innerHTML = html;
                showNotification('获取热门文章失败，显示模拟数据', 'error');
            });
    }
    
    // 加载标签
    function loadTags() {
        const tagsCloud = document.getElementById('tagsCloud');
        if (!tagsCloud) return;
        
        // 发送请求到后端获取标签列表
        fetch('/api/tags')
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                const tags = data.tags || [];
                
                if (tags.length === 0) {
                    tagsCloud.innerHTML = '<p>暂无标签</p>';
                    return;
                }
                
                let html = '';
                tags.forEach(tag => {
                    const fontSize = 0.8 + (tag.count / 20) * 0.4; // 根据数量调整大小
                    html += `
                    <a href="javascript:void(0)" 
                    class="category-tag" 
                    data-tag="${tag.name}"
                    style="font-size: ${fontSize}rem">
                        ${tag.name} (${tag.count})
                    </a>
                    `;
                });
                
                tagsCloud.innerHTML = html;

                // 为标签添加点击事件
                const tagLinks = tagsCloud.querySelectorAll('.category-tag');
                tagLinks.forEach(link => {
                    link.addEventListener('click', function(e) {
                        e.preventDefault();
                        const tagName = this.getAttribute('data-tag');
                        loadArticlesByTag(tagName);
                    });
                });
            })
            .catch(error => {
                console.error('获取标签列表失败:', error);
                // 使用模拟数据作为后备方案
                const tags = [
                    { name: 'JavaScript', count: 15 },
                    { name: 'CSS', count: 8 },
                    { name: 'HTML', count: 12 },
                    { name: 'Python', count: 10 },
                    { name: 'Java', count: 7 },
                    { name: 'React', count: 14 },
                    { name: 'Vue', count: 9 },
                    { name: 'Node.js', count: 6 },
                    { name: 'MongoDB', count: 5 },
                    { name: '算法', count: 8 },
                    { name: '面试', count: 11 }
                ];
                
                let html = '';
                tags.forEach(tag => {
                    const fontSize = 0.8 + (tag.count / 20) * 0.4;
                    html += `
                    <a href="javascript:void(0)" 
                    class="category-tag" 
                    data-tag="${tag.name}"
                    style="font-size: ${fontSize}rem">
                        ${tag.name} (${tag.count})
                    </a>
                    `;
                });
                
                tagsCloud.innerHTML = html;

                // 为模拟数据的标签也添加点击事件
                const tagLinks = tagsCloud.querySelectorAll('.category-tag');
                tagLinks.forEach(link => {
                    link.addEventListener('click', function(e) {
                        e.preventDefault();
                        const tagName = this.getAttribute('data-tag');
                        loadArticlesByTag(tagName);
                    });
                });

                showNotification('获取标签列表失败，显示模拟数据', 'error');
            });
    }

    // 添加加载特定标签文章的函数
    function loadArticlesByTag(tag) {
        showLoading();
        currentPage = 1; // 重置页码
        
        // 更新当前分类显示
        const currentCategoryEl = document.getElementById('currentCategory');
        if (currentCategoryEl) {
            currentCategoryEl.textContent = `标签: ${tag}`;
        }
        
        // 更新URL但不刷新页面
        const url = new URL(window.location);
        url.searchParams.set('tag', tag);
        window.history.pushState({}, '', url);
        
        // 发送请求到后端获取特定标签的文章
        fetch(`/api/articles?tag=${encodeURIComponent(tag)}&page=${currentPage}&size=${articlesPerPage}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('网络响应异常');
                }
                return response.json();
            })
            .then(data => {
                articles = data.articles || [];
                totalPages = data.totalPages || 1;
                
                renderArticles();
                renderPagination();
                hideLoading();
                
                // 滚动到文章列表
                const categoryArticles = document.getElementById('categoryArticles');
                if (categoryArticles) {
                    categoryArticles.scrollIntoView({ behavior: 'smooth' });
                }
            })
            .catch(error => {
                console.error(`获取标签 "${tag}" 的文章失败:`, error);
                // 使用模拟数据作为后备方案
                let allMockArticles = generateMockArticles(30);
                
                // 为模拟数据添加标签
                allMockArticles = allMockArticles.map(article => ({
                    ...article,
                    tags: [tag, '其他标签'] // 确保当前标签在文章的标签列表中
                }));
                
                // 过滤出包含特定标签的文章
                allMockArticles = allMockArticles.filter(article => 
                    article.tags && article.tags.includes(tag)
                );
                
                totalPages = Math.ceil(allMockArticles.length / articlesPerPage);
                
                // 手动分页
                const startIdx = (currentPage - 1) * articlesPerPage;
                const endIdx = Math.min(startIdx + articlesPerPage, allMockArticles.length);
                articles = allMockArticles.slice(startIdx, endIdx);
                
                renderArticles();
                renderPagination();
                hideLoading();
                
                showNotification(`获取标签 "${tag}" 的文章失败，显示模拟数据`, 'error');
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
        const userName = document.getElementById('userName');
        const userAvatar = document.getElementById('userAvatar');
        const articleCount = document.getElementById('articleCount');
        const categoryCount = document.getElementById('categoryCount');
        const likeCount = document.getElementById('likeCount');
        const userDropdown = document.querySelector('.user-dropdown');
        
        if (isLoggedIn && userData) {
            // 用户已登录，显示用户信息
            if (userName) userName.textContent = userData.username || '用户';
            if (userAvatar) userAvatar.src = userData.avatar || 'img/default_touxiang.jpg';
            if (articleCount) articleCount.textContent = userData.articleCount || '0';
            if (categoryCount) categoryCount.textContent = userData.viewCount || '0';
            if (likeCount) likeCount.textContent = userData.likeCount || '0';
            
            // 更新下拉菜单
            if (userDropdown) {
                userDropdown.innerHTML = `
                <a href="user_center.html"><i class="fas fa-user-circle"></i> 个人中心</a>
                <a href="blog_editor.html"><i class="fas fa-edit"></i> 写博客</a>
                <a href="new_user_settings.html"><i class="fas fa-cog"></i> 设置</a>
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
            if (userName) userName.textContent = '游客';
            if (userAvatar) userAvatar.src = 'img/default_touxiang.jpg';
            if (articleCount) articleCount.textContent = '0';
            if (categoryCount) categoryCount.textContent = '0';
            if (likeCount) likeCount.textContent = '0';
            
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
            // 更新UI为未登录状态
            updateUIForAuthStatus(false);
        })
        .catch(error => {
            console.error('退出登录失败:', error);
            showNotification('退出登录请求失败，请重试', 'error');
        });
    }
    
    // 切换视图
    function changeView(view) {
        if (view === currentView) return;
        
        currentView = view;
        renderArticles();
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
    
    // 显示分类加载中
    function showCategoriesLoading() {
        const categoriesContainer = document.getElementById('categoriesContainer');
        if (!categoriesContainer) return;
        
        let skeletonHTML = '';
        for (let i = 0; i < 10; i++) {
            skeletonHTML += `
            <div class="category-card skeleton-card">
                <div class="skeleton-icon skeleton"></div>
                <div class="skeleton-title skeleton"></div>
                <div class="skeleton-text skeleton"></div>
            </div>
            `;
        }
        
        categoriesContainer.innerHTML = skeletonHTML;
    }
    
    // 显示文章加载中
    function showLoading() {
        const articlesContainer = document.getElementById('articlesContainer');
        if (!articlesContainer) return;
        
        let skeletonHTML = '';
        for (let i = 0; i < articlesPerPage; i++) {
            skeletonHTML += `
            <div class="skeleton-card">
                <div class="skeleton-img skeleton"></div>
                <div class="skeleton-title skeleton"></div>
                <div class="skeleton-text skeleton"></div>
                <div class="skeleton-text skeleton"></div>
                <div class="skeleton-text skeleton"></div>
            </div>
            `;
        }
        
        articlesContainer.innerHTML = skeletonHTML;
    }
    
    // 隐藏加载状态
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
    
    // 生成模拟分类数据
    function generateMockCategories() {
        return [
            { name: '技术', count: 15 },
            { name: '生活', count: 8 },
            { name: '旅行', count: 6 },
            { name: '美食', count: 10 },
            { name: '编程', count: 20 },
            { name: '前端', count: 12 },
            { name: '后端', count: 9 },
            { name: '设计', count: 7 },
            { name: '数据库', count: 5 },
            { name: '算法', count: 8 }
        ];
    }
    
    // 生成模拟文章数据
    function generateMockArticles(count) {
        const mockArticles = [];
        const categories = ['技术', '生活', '旅行', '美食', '编程', '前端', '后端', '设计', '数据库', '算法'];
        const authors = [
            { name: '张三', avatar: 'img/avatar1.jpg' },
            { name: '李四', avatar: 'img/avatar2.jpg' },
            { name: '王五', avatar: 'img/avatar3.jpg' }
        ];
        
        for (let i = 1; i <= count; i++) {
            const views = Math.floor(Math.random() * 1000);
            const likes = Math.floor(Math.random() * 200);
            const comments = Math.floor(Math.random() * 50);
            const author = authors[Math.floor(Math.random() * authors.length)];
            const category = categories[Math.floor(Math.random() * categories.length)];
            
            mockArticles.push({
                id: i,
                title: `博客文章标题 ${i}: 这是一个测试博客文章的标题`,
                excerpt: '这是文章的摘要内容，通常显示文章的前几段文字。这只是一个示例文本，用于展示文章卡片的样式和布局效果。',
                content: '这是文章的完整内容...',
                thumbnail: `https://picsum.photos/600/400?random=${i}`,
                author: author.name,
                authorAvatar: author.avatar,
                date: new Date(Date.now() - Math.floor(Math.random() * 30) * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
                category: category,
                views: views,
                likes: likes,
                comments: comments
            });
        }
        
        return mockArticles;
    }

    // 添加更新分类选中状态的函数
    function updateCategorySelection(categoryName) {
        const categoriesContainer = document.getElementById('categoriesContainer');
        if (!categoriesContainer) return;
        
        // 移除所有卡片的active类
        const allCards = categoriesContainer.querySelectorAll('.category-card');
        allCards.forEach(card => card.classList.remove('active'));
        
        // 为当前分类添加active类
        const selectedCard = categoriesContainer.querySelector(
            `.category-card[data-category="${categoryName}"]`
        );
        if (selectedCard) {
            selectedCard.classList.add('active');
        }
        
        // 更新当前分类标题
        const currentCategoryEl = document.getElementById('currentCategory');
        if (currentCategoryEl) {
            currentCategoryEl.textContent = categoryName || '所有分类';
        }
    }

    // 添加浏览器历史记录状态处理
    window.addEventListener('popstate', function(event) {
        const urlParams = new URLSearchParams(window.location.search);
        const categoryParam = urlParams.get('category');
        const tagParam = urlParams.get('tag');
        
        if (tagParam) {
            loadArticlesByTag(decodeURIComponent(tagParam));
        } else if (categoryParam) {
            currentCategory = decodeURIComponent(categoryParam);
            updateCategorySelection(currentCategory);
            loadArticlesByCategory(currentCategory);
        } else {
            currentCategory = '';
            updateCategorySelection('');
            loadArticles();
        }
    });
});