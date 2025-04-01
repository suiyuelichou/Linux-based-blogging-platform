document.addEventListener('DOMContentLoaded', function() {
    // 定义全局变量
    let tags = []; // 将tags变量提前声明，避免ReferenceError
    
    // 初始化编辑器
    const quill = new Quill('#editor', {
        theme: 'snow',
        placeholder: '开始撰写你的博客内容...',
        modules: {
            toolbar: {
                container: [
                    [{ 'header': [1, 2, 3, 4, 5, 6, false] }],
                    ['bold', 'italic', 'underline', 'strike'],
                    [{ 'color': [] }, { 'background': [] }],
                    [{ 'align': [] }],
                    ['blockquote', 'code-block'],
                    [{ 'list': 'ordered'}, { 'list': 'bullet' }],
                    ['link', 'image'],
                    ['clean']
                ],
                handlers: {
                    'image': function() {
                        // 触发文件选择
                        const input = document.createElement('input');
                        input.setAttribute('type', 'file');
                        input.setAttribute('accept', 'image/*');
                        input.click();

                        input.onchange = async () => {
                            const file = input.files[0];
                            if (file) {
                                try {
                                    const formData = new FormData();
                                    formData.append('image', file);

                                    // 显示上传进度提示
                                    showNotification('正在上传图片...', 'info');

                                    const response = await fetch('/api/upload/image', {
                                        method: 'POST',
                                        credentials: 'same-origin',
                                        body: formData
                                    });

                                    if (!response.ok) {
                                        throw new Error('图片上传失败');
                                    }

                                    const data = await response.json();
                                    if (data.success) {
                                        // 获取光标位置
                                        const range = quill.getSelection(true);
                                        
                                        // 修改这里：移除URL中的引号
                                        const imageUrl = data.url.replace(/['"]/g, ''); // 移除所有引号
                                        
                                        // 插入图片
                                        quill.insertEmbed(range.index, 'image', imageUrl);
                                        // 光标移动到图片后面
                                        quill.setSelection(range.index + 1);
                                        showNotification('图片上传成功', 'success');
                                    } else {
                                        throw new Error(data.message || '图片上传失败');
                                    }
                                } catch (error) {
                                    console.error('图片上传失败:', error);
                                    showNotification('图片上传失败: ' + error.message, 'error');
                                }
                            }
                        };
                    }
                }
            }
        }

    });
    // 调用函数添加工具提示
    addEditorButtonTooltips();

    // 添加编辑器按钮提示功能
    function addEditorButtonTooltips() {
        // 等待编辑器完全加载
        setTimeout(() => {
            // 为格式按钮添加提示
            const formatButtons = {
                '.ql-bold': '加粗 (Ctrl+B)',
                '.ql-italic': '斜体 (Ctrl+I)',
                '.ql-underline': '下划线 (Ctrl+U)',
                '.ql-strike': '删除线',
                '.ql-blockquote': '引用块',
                '.ql-code-block': '代码块',
                '.ql-link': '插入链接 (Ctrl+K)',
                '.ql-image': '插入图片',
                '.ql-list[value="ordered"]': '有序列表',
                '.ql-list[value="bullet"]': '无序列表',
                '.ql-clean': '清除格式'
            };
            
            // 为标题选择器添加提示
            document.querySelector('.ql-header').setAttribute('title', '标题格式');
            
            // 为颜色选择器添加提示
            document.querySelector('.ql-color').setAttribute('title', '文字颜色');
            document.querySelector('.ql-background').setAttribute('title', '背景颜色');
            
            // 为对齐方式按钮添加提示
            document.querySelector('.ql-align').setAttribute('title', '文本对齐');
            
            // 为所有格式按钮添加提示
            for (const selector in formatButtons) {
                const button = document.querySelector(selector);
                if (button) {
                    button.setAttribute('title', formatButtons[selector]);
                }
            }
        }, 500); // 给予足够时间让编辑器加载完成
    }
    
    // 检查是否是编辑模式
    const urlParams = new URLSearchParams(window.location.search);
    const isEditMode = urlParams.get('mode') === 'edit';
    const blogId = urlParams.get('id');
    
    // 更新页面标题和按钮文本
    const editorTitle = document.querySelector('.editor-title');
    const publishBtn = document.getElementById('publishBtn');
    
    if (isEditMode) {
        editorTitle.textContent = '编辑博客';
        publishBtn.innerHTML = '<i class="fas fa-save"></i> 保存更改';
        
        // 从 sessionStorage 获取博客数据
        const blogDataJson = sessionStorage.getItem('editBlogData');
        if (blogDataJson) {
            try {
                const blogData = JSON.parse(blogDataJson);
                console.log('从sessionStorage获取的博客数据:', blogData); // 调试日志
                
                // 填充博客数据
                fillBlogData(blogData);
                
                // 清除 sessionStorage 中的数据
                sessionStorage.removeItem('editBlogData');
            } catch (error) {
                console.error('解析博客数据失败:', error);
                showNotification('解析博客数据失败', 'error');
            }
        } else {
            console.warn('未找到sessionStorage中的博客数据');
            showNotification('未找到博客数据', 'warning');
        }
    }
    
    // 填充博客数据的函数
    function fillBlogData(blogData) {
        console.log('开始填充博客数据'); // 调试日志
        
        // 统一封面图片字段名
        if (!blogData.coverImage) {
            // 尝试其他可能的字段名
            blogData.coverImage = blogData.cover_image || blogData.thumbnail || blogData.cover || '';
            console.log('统一封面图片字段:', blogData.coverImage); // 调试日志
        }
        
        // 确保统一标签字段名
        if (!blogData.tags || !Array.isArray(blogData.tags)) {
            // 尝试其他可能的字段名
            let foundTags = null;
            if (blogData.tag_list && Array.isArray(blogData.tag_list)) {
                foundTags = blogData.tag_list;
            } else if (blogData.tag && Array.isArray(blogData.tag)) {
                foundTags = blogData.tag;
            } else if (blogData.tagList && Array.isArray(blogData.tagList)) {
                foundTags = blogData.tagList;
            } else if (typeof blogData.tags === 'string') {
                // 如果是JSON字符串，尝试解析
                try {
                    foundTags = JSON.parse(blogData.tags);
                } catch(e) {
                    // 如果不是有效的JSON，可能是逗号分隔的标签字符串
                    foundTags = blogData.tags.split(',').map(tag => tag.trim()).filter(tag => tag);
                }
            }
            
            // 如果找到了标签，更新字段
            if (foundTags) {
                blogData.tags = foundTags;
                console.log('统一标签字段:', foundTags); // 调试日志
            } else {
                // 默认为空数组
                blogData.tags = [];
                console.log('未找到标签数据，使用空数组'); // 调试日志
            }
        }
        
        // 设置标题
        document.getElementById('blogTitle').value = blogData.title || '';
        console.log('已设置标题:', blogData.title); // 调试日志
        
        // 设置内容
        if (blogData.content) {
            console.log('设置内容，格式:', blogData.content_format); // 调试日志
            if (blogData.content_format === 'markdown') {
                // 如果是 Markdown 格式，先转换为 HTML
                try {
                    const html = marked.parse(blogData.content);
                    quill.root.innerHTML = html;
                    console.log('Markdown已转换为HTML并设置'); // 调试日志
                } catch (error) {
                    console.error('Markdown转换失败:', error);
                    quill.root.innerHTML = blogData.content;
                }
            } else {
                quill.root.innerHTML = blogData.content;
                console.log('已设置HTML内容'); // 调试日志
            }
        }
        
        // 展开高级选项面板
        const advancedOptionsPanel = document.getElementById('advancedOptionsPanel');
        const advancedOptionsBtn = document.getElementById('advancedOptionsBtn');
        if (advancedOptionsPanel.classList.contains('hidden')) {
            advancedOptionsPanel.classList.remove('hidden');
            advancedOptionsBtn.innerHTML = '<i class="fas fa-times"></i> 收起设置面板';
            console.log('已展开高级选项面板'); // 调试日志
            
            // 加载分类
            loadCategories();
        }
        
        // 设置分类
        if (blogData.category) {
            console.log('设置分类:', blogData.category); // 调试日志
            setTimeout(() => {
                const categorySelect = document.getElementById('blogCategory');
                // 等待分类加载完成后设置
                const checkCategoryInterval = setInterval(() => {
                    if (categorySelect.options.length > 1) {
                        for (let i = 0; i < categorySelect.options.length; i++) {
                            if (categorySelect.options[i].value === blogData.category) {
                                categorySelect.selectedIndex = i;
                                console.log('已选择分类:', categorySelect.options[i].text); // 调试日志
                                break;
                            }
                        }
                        clearInterval(checkCategoryInterval);
                    }
                }, 100);
            }, 500); // 给分类加载一些时间
        }
        
        // 设置标签 - 先重置全局tags数组和清空标签容器
        tags = []; // 重置全局tags数组
        
        // 获取标签容器并清空现有标签元素
        const tagElements = document.querySelectorAll('#tagsContainer .tag');
        tagElements.forEach(tag => tag.remove());
        
        // 然后添加标签
        if (blogData.tags && Array.isArray(blogData.tags)) {
            console.log('设置标签:', blogData.tags); // 调试日志
            blogData.tags.forEach(tag => {
                if (tag && typeof tag === 'string' && !tags.includes(tag)) {
                    addTag(tag);
                }
            });
        }
        
        // 设置封面图片
        if (blogData.coverImage) {
            console.log('设置封面图片:', blogData.coverImage); // 调试日志
            const thumbnailPreview = document.getElementById('thumbnailPreview');
            thumbnailPreview.innerHTML = `<img src="${blogData.coverImage}" alt="封面预览">`;
            thumbnailPreview.classList.remove('no-image');
        }
        
        console.log('博客数据填充完成'); // 调试日志
    }
    
    // 修改发布/保存按钮的处理逻辑
    publishBtn.addEventListener('click', function() {
        // 表单验证
        const title = document.getElementById('blogTitle').value;
        const content = quill.root.innerHTML;
        
        if (!title) {
            showNotification('请输入文章标题', 'error');
            return;
        }
        
        if (quill.getText().trim().length < 10) {
            showNotification('文章内容太短，请至少输入10个字', 'error');
            return;
        }
        
        // 显示确认对话框
        const modalTitle = document.getElementById('modalTitle');
        const modalBody = document.getElementById('modalBody');
        const modalConfirmBtn = document.getElementById('modalConfirmBtn');
        
        modalTitle.textContent = isEditMode ? '确认保存' : '确认发布';
        modalBody.innerHTML = isEditMode ? 
            '<p>确定要保存这些更改吗？</p>' :
            '<p>确定要发布这篇文章吗？发布后可以在个人中心编辑。</p>';
        modalConfirmBtn.textContent = isEditMode ? '保存' : '发布';
        
        // 显示确认对话框
        const confirmModal = document.getElementById('confirmModal');
        confirmModal.style.display = 'block';
        setTimeout(() => {
            confirmModal.classList.add('show');
        }, 10);
    });
    
    // 修改确认按钮的处理逻辑
    document.getElementById('modalConfirmBtn').addEventListener('click', function() {
        if (isEditMode) {
            updateArticle(blogId);
        } else {
            publishArticle();
        }
    });
    
    // 添加取消按钮的事件处理
    document.getElementById('modalCancelBtn').addEventListener('click', function() {
        closeConfirmModal();
    });
    
    // 添加关闭按钮(×)的事件处理
    document.querySelector('.close-modal').addEventListener('click', function() {
        closeConfirmModal();
    });
    
    // 关闭确认模态框的函数
    function closeConfirmModal() {
        const confirmModal = document.getElementById('confirmModal');
        confirmModal.classList.remove('show');
        setTimeout(() => {
            confirmModal.style.display = 'none';
        }, 300);
    }
    
    // 点击模态框外部区域关闭
    document.getElementById('confirmModal').addEventListener('click', function(e) {
        if (e.target === this) {
            closeConfirmModal();
        }
    });
    
    // 发布文章的函数
    async function publishArticle() {
        const title = document.getElementById('blogTitle').value;
        const content = quill.root.innerHTML;
        
        try {
            // 显示发布中状态
            const modalConfirmBtn = document.getElementById('modalConfirmBtn');
            modalConfirmBtn.disabled = true;
            modalConfirmBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 发布中...';
            
            // 准备表单数据
            const formData = new FormData();
            formData.append('title', title);
            formData.append('content', content);
            
            // 只有选择了分类时才添加分类字段
            const category = document.getElementById('blogCategory').value;
            if (category) {
                formData.append('category', category);
            }
            
            // 只有添加了标签时才添加标签字段
            const tagElements = document.querySelectorAll('#tagsContainer .tag');
            if (tagElements.length > 0) {
                const tagsList = Array.from(tagElements)
                    .map(tag => tag.textContent.trim().replace(/×$/, '').trim());
                formData.append('tags', JSON.stringify(tagsList));
            }
            
            // 只有上传了封面图片时才添加图片字段
            const thumbnailFile = document.getElementById('thumbnailUpload').files[0];
            if (thumbnailFile) {
                formData.append('thumbnail', thumbnailFile);
            }
            
            // 发送发布请求
            const response = await fetch('/api/articles', {
                method: 'POST',
                body: formData
            });
            
            if (!response.ok) {
                throw new Error('发布博客失败');
            }
            
            const result = await response.json();
            
            // 关闭确认对话框
            closeConfirmModal();
            
            showNotification('博客发布成功', 'success');
            
            // 延迟跳转到博客详情页
            setTimeout(() => {
                // 尝试从多种可能的返回格式中获取博客ID
                let articleId = null;
                
                if (result && result.articleId) {
                    articleId = result.articleId;
                } else if (result && result.id) {
                    articleId = result.id;
                } else if (result && result.blogId) {
                    articleId = result.blogId;
                } else if (result && result.data && result.data.id) {
                    articleId = result.data.id;
                } else if (result && result.data && result.data.articleId) {
                    articleId = result.data.articleId;
                } else if (result && result.data && result.data.blogId) {
                    articleId = result.data.blogId;
                }
                
                if (articleId) {
                    console.log('跳转到博客详情页:', articleId);
                    window.location.href = `blog_detail.html?id=${articleId}`;
                } else {
                    console.warn('未能获取博客ID，跳转到首页');
                    window.location.href = 'blog_home.html';
                }
            }, 1500);
            
        } catch (error) {
            console.error('发布博客失败:', error);
            showNotification('发布失败，请重试', 'error');
            
            // 恢复按钮状态
            const modalConfirmBtn = document.getElementById('modalConfirmBtn');
            modalConfirmBtn.disabled = false;
            modalConfirmBtn.textContent = '发布';
        }
    }
    
    // 更新文章的函数
    async function updateArticle(blogId) {
        const title = document.getElementById('blogTitle').value;
        const content = quill.root.innerHTML;
        
        try {
            // 显示保存中状态
            const modalConfirmBtn = document.getElementById('modalConfirmBtn');
            modalConfirmBtn.disabled = true;
            modalConfirmBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 保存中...';
            
            // 准备表单数据
            const formData = new FormData();
            formData.append('title', title);
            formData.append('content', content);
            
            // 只有选择了分类时才添加分类字段
            const category = document.getElementById('blogCategory').value;
            if (category) {
                formData.append('category', category);
            }
            
            // 只有添加了标签时才添加标签字段
            const tagElements = document.querySelectorAll('#tagsContainer .tag');
            if (tagElements.length > 0) {
                const tagsList = Array.from(tagElements)
                    .map(tag => tag.textContent.trim().replace(/×$/, '').trim());
                formData.append('tags', JSON.stringify(tagsList));
            }
            
            // 处理封面图片
            const thumbnailFile = document.getElementById('thumbnailUpload').files[0];
            let hasCoverImage = false;
            
            if (thumbnailFile) {
                // 如果有新选择的图片，上传新图片
                formData.append('thumbnail', thumbnailFile);
                hasCoverImage = true;
            } else {
                // 如果没有新选择的图片，但有现有的封面图片，传递原有的URL
                const thumbnailImg = document.querySelector('#thumbnailPreview img');
                if (thumbnailImg && thumbnailImg.src) {
                    try {
                        // 获取相对路径 - 从URL中提取
                        let coverImageUrl = thumbnailImg.src;
                        
                        // 处理绝对URL转为相对路径
                        // 例如从 http://localhost:3000/thumbnail/image.jpg 转为 /thumbnail/image.jpg
                        if (coverImageUrl.includes('/thumbnail/')) {
                            const pathMatch = coverImageUrl.match(/\/thumbnail\/[^?#]*/);
                            if (pathMatch) {
                                coverImageUrl = pathMatch[0];
                            }
                        }
                        
                        // 如果是数据URL (base64)，我们需要上传该图像
                        if (coverImageUrl.startsWith('data:image')) {
                            // 保留原始文件格式
                            formData.append('thumbnailBase64', coverImageUrl);
                        } else {
                            // 使用relative_path作为表单字段名，以便后端区分
                            formData.append('thumbnail_path', coverImageUrl);
                        }
                        
                        hasCoverImage = true;
                        console.log('保留原有封面图片:', coverImageUrl);
                    } catch (error) {
                        console.error('处理封面图片URL失败:', error);
                    }
                }
            }
            
            // 如果高级面板被打开但没有封面图片，且预览区没有图片，明确指出移除封面
            const advancedPanelVisible = !document.getElementById('advancedOptionsPanel').classList.contains('hidden');
            const noPreviewImage = document.getElementById('thumbnailPreview').classList.contains('no-image');
            
            if (advancedPanelVisible && !hasCoverImage && noPreviewImage) {
                formData.append('remove_thumbnail', 'true');
            }
            
            // 发送更新请求
            const response = await fetch(`/api/blogs/${blogId}`, {
                method: 'PATCH',
                body: formData
            });
            
            if (!response.ok) {
                throw new Error('更新博客失败');
            }
            
            const result = await response.json();
            
            // 关闭确认对话框
            closeConfirmModal();
            
            showNotification('博客更新成功', 'success');
            
            // 延迟跳转到博客详情页
            setTimeout(() => {
                // 尝试从多种可能的返回格式中获取博客ID
                let articleId = null;
                
                if (result && result.articleId) {
                    articleId = result.articleId;
                } else if (result && result.id) {
                    articleId = result.id;
                } else if (result && result.blogId) {
                    articleId = result.blogId;
                } else if (result && result.data && result.data.id) {
                    articleId = result.data.id;
                } else if (result && result.data && result.data.articleId) {
                    articleId = result.data.articleId;
                } else if (result && result.data && result.data.blogId) {
                    articleId = result.data.blogId;
                } else {
                    // 如果无法从返回结果中获取ID，使用URL中的博客ID
                    articleId = blogId;
                }
                
                if (articleId) {
                    console.log('跳转到博客详情页:', articleId);
                    window.location.href = `blog_detail.html?id=${articleId}`;
                } else {
                    console.warn('未能获取博客ID，跳转到首页');
                    window.location.href = 'blog_home.html';
                }
            }, 1500);
            
        } catch (error) {
            console.error('保存博客失败:', error);
            showNotification('保存失败，请重试', 'error');
            
            // 恢复按钮状态
            const modalConfirmBtn = document.getElementById('modalConfirmBtn');
            modalConfirmBtn.disabled = false;
            modalConfirmBtn.textContent = '保存';
        }
    }
    
    // 设置暗黑模式
    setupDarkMode();
    
    // 移动端菜单切换
    const mobileToggle = document.querySelector('.mobile-toggle');
    const mainNav = document.querySelector('.main-nav');
    
    if (mobileToggle) {
        mobileToggle.addEventListener('click', function() {
            mainNav.classList.toggle('active');
            this.classList.toggle('active');
        });
    }
    
    // 标签添加功能
    const tagsInputElement = document.getElementById('tagsInput');
    if (tagsInputElement) {
        tagsInputElement.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' || e.key === ',') {
                e.preventDefault();
                const tagValue = this.value.trim();
                if (tagValue && tags.length < 5 && !tags.includes(tagValue)) {
                    addTag(tagValue);
                    this.value = '';
                } else if (tags.length >= 5) {
                    showNotification('最多添加5个标签', 'error');
                } else if (tags.includes(tagValue)) {
                    showNotification('标签已存在', 'error');
                }
            }
        });
    }
    
    function addTag(tagValue) {
        // 确保tags数组已初始化
        if (!Array.isArray(tags)) {
            tags = [];
        }
        
        // 检查是否已存在相同标签
        if (tags.includes(tagValue)) {
            return; // 如果标签已存在，则直接返回
        }
        
        // 在函数内部获取DOM元素，而不是依赖外部变量
        const tagsContainer = document.getElementById('tagsContainer');
        const tagsInput = document.getElementById('tagsInput');
        
        if (!tagsContainer || !tagsInput) {
            console.error('找不到标签容器或输入框元素');
            return;
        }
        
        tags.push(tagValue);
        
        const tagElement = document.createElement('span');
        tagElement.className = 'tag';
        tagElement.innerHTML = `${tagValue} <span class="remove-tag">&times;</span>`;
        
        const removeBtn = tagElement.querySelector('.remove-tag');
        removeBtn.addEventListener('click', function() {
            tags = tags.filter(tag => tag !== tagValue);
            tagElement.remove();
        });
        
        tagsContainer.insertBefore(tagElement, tagsInput);
    }
    
    // 封面图片预览
    const thumbnailUpload = document.getElementById('thumbnailUpload');
    const thumbnailPreview = document.getElementById('thumbnailPreview');
    
    thumbnailUpload.addEventListener('change', async function(e) {
        const file = this.files[0];
        if (file) {
            // 检查文件大小
            if (file.size > 14 * 1024) { // 大于14kb
                try {
                    // 压缩图片
                    const compressedFile = await compressImage(file);
                    
                    // 检查压缩后的大小
                    if (compressedFile.size > 14 * 1024) {
                        showNotification('图片大小超过14KB，即使压缩后仍然过大，请选择更小的图片', 'error');
                        this.value = ''; // 清空输入
                        thumbnailPreview.innerHTML = `<span>尚未上传封面图片</span>`;
                        thumbnailPreview.classList.add('no-image');
                        return;
                    }
                    
                    // 显示压缩后的预览
                    const reader = new FileReader();
                    reader.onload = function(e) {
                        thumbnailPreview.innerHTML = `<img src="${e.target.result}" alt="封面预览">`;
                        thumbnailPreview.classList.remove('no-image');
                    };
                    reader.readAsDataURL(compressedFile);
                } catch (error) {
                    console.error('图片压缩失败:', error);
                    showNotification('图片压缩失败，请重试', 'error');
                    this.value = '';
                }
            } else {
                // 文件小于14kb，直接显示预览
                const reader = new FileReader();
                reader.onload = function(e) {
                    thumbnailPreview.innerHTML = `<img src="${e.target.result}" alt="封面预览">`;
                    thumbnailPreview.classList.remove('no-image');
                };
                reader.readAsDataURL(file);
            }
        }
    });
    
    // 图片压缩函数
    function compressImage(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.readAsDataURL(file);
            
            reader.onload = function(e) {
                const img = new Image();
                img.src = e.target.result;
                
                img.onload = function() {
                    const canvas = document.createElement('canvas');
                    const ctx = canvas.getContext('2d');
                    
                    // 计算压缩后的尺寸，保持宽高比
                    let width = img.width;
                    let height = img.height;
                    const maxSize = 800; // 最大尺寸
                    
                    if (width > height && width > maxSize) {
                        height = Math.round((height * maxSize) / width);
                        width = maxSize;
                    } else if (height > maxSize) {
                        width = Math.round((width * maxSize) / height);
                        height = maxSize;
                    }
                    
                    canvas.width = width;
                    canvas.height = height;
                    
                    // 绘制压缩后的图片
                    ctx.drawImage(img, 0, 0, width, height);
                    
                    // 转换为Blob
                    canvas.toBlob(
                        (blob) => {
                            resolve(new File([blob], file.name, {
                                type: 'image/jpeg',
                                lastModified: Date.now()
                            }));
                        },
                        'image/jpeg',
                        0.7 // 压缩质量，0.7通常是一个好的平衡点
                    );
                };
                
                img.onerror = function() {
                    reject(new Error('图片加载失败'));
                };
            };
            
            reader.onerror = function() {
                reject(new Error('文件读取失败'));
            };
        });
    }
    
    // 字数统计和阅读时间
    const wordCountEl = document.getElementById('wordCount');
    const readTimeEl = document.getElementById('readTime');
    
    quill.on('text-change', function() {
        const text = quill.getText();
        const wordCount = text.trim().length;
        const readTime = Math.ceil(wordCount / 400); // 假设每分钟阅读400字
        
        wordCountEl.textContent = wordCount;
        readTimeEl.textContent = readTime;
    });
    
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
    
    // 检查登录状态
    checkAuthStatus().then(isLoggedIn => {
        if (isLoggedIn) {
            // 如果用户已登录，预加载分类
            loadCategories();
        }
    });
    
    // 设置暗黑模式
    function setupDarkMode() {
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
    
    // 检查用户登录状态
    function checkAuthStatus() {
        return new Promise((resolve) => {
            fetch('/api/user/info')
                .then(response => {
                    if (!response.ok) {
                        if (response.status === 401) {
                            // 用户未登录
                            showLoginRequiredMessage();
                            updateUIForAuthStatus(false);
                            resolve(false);
                            return null;
                        }
                        throw new Error('网络响应异常');
                    }
                    return response.json();
                })
                .then(data => {
                    if (data) {
                        // 用户已登录
                        updateUIForAuthStatus(true, data);
                        resolve(true);
                    } else {
                        resolve(false);
                    }
                })
                .catch(error => {
                    console.error('检查登录状态失败:', error);
                    updateUIForAuthStatus(false);
                    resolve(false);
                });
        });
    }
    
    // 显示登录提示
    function showLoginRequiredMessage() {
        const editorContainer = document.querySelector('.editor-container');
        
        const loginMessage = document.createElement('div');
        loginMessage.className = 'login-required-message';
        loginMessage.innerHTML = `
            <div class="message-content">
                <i class="fas fa-lock"></i>
                <h3>请先登录</h3>
                <p>登录后才能发布博客文章</p>
                <a href="blog_login.html" class="login-btn">立即登录</a>
                <a href="blog_register.html" class="register-link">没有账号？立即注册</a>
            </div>
        `;
        
        // 添加样式
        const style = document.createElement('style');
        style.textContent = `
            .login-required-message {
                background-color: var(--card-bg);
                border-radius: 8px;
                padding: 30px;
                text-align: center;
                box-shadow: 0 4px 15px rgba(0,0,0,0.1);
                margin: 20px auto;
                max-width: 400px;
            }
            
            .message-content i {
                font-size: 3rem;
                color: var(--primary-color);
                margin-bottom: 15px;
            }
            
            .message-content h3 {
                font-size: 1.5rem;
                margin-bottom: 10px;
                color: var(--text-primary);
            }
            
            .message-content p {
                color: var(--text-secondary);
                margin-bottom: 20px;
            }
            
            .login-btn {
                display: inline-block;
                padding: 10px 20px;
                background-color: var(--primary-color);
                color: white;
                border-radius: 5px;
                text-decoration: none;
                font-weight: 500;
                margin-bottom: 15px;
                transition: var(--transition);
            }
            
            .login-btn:hover {
                background-color: var(--primary-dark);
            }
            
            .register-link {
                display: block;
                color: var(--primary-color);
                text-decoration: none;
            }
            
            .register-link:hover {
                text-decoration: underline;
            }
        `;
        
        document.head.appendChild(style);
        
        // 隐藏编辑器并显示登录消息
        editorContainer.innerHTML = '';
        editorContainer.appendChild(loginMessage);
        
        // 禁用发布按钮
        document.getElementById('publishBtn').disabled = true;
    }
    
    // 根据登录状态更新UI
    function updateUIForAuthStatus(isLoggedIn, userData) {
        const userName = document.getElementById('userName');
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
        fetch('/api/user/logout', {
            method: 'POST',
            credentials: 'same-origin'
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
            // 跳转到首页
            setTimeout(() => {
                window.location.href = 'blog_home.html';
            }, 1500);
        })
        .catch(error => {
            console.error('退出登录失败:', error);
            showNotification('退出登录请求失败，请重试', 'error');
        });
    }
    
    // 显示通知消息
    function showNotification(message, type = 'info') {
        const notification = document.getElementById('notification');
        const notificationIcon = notification.querySelector('.notification-icon');
        const notificationMessage = notification.querySelector('.notification-message');
        
        // 设置图标
        switch (type) {
            case 'success':
                notificationIcon.className = 'notification-icon fas fa-check-circle';
                break;
            case 'error':
                notificationIcon.className = 'notification-icon fas fa-exclamation-circle';
                break;
            default:
                notificationIcon.className = 'notification-icon fas fa-info-circle';
        }
        
        // 设置通知类型和消息
        notification.className = `notification ${type}`;
        notificationMessage.textContent = message;
        
        // 显示通知
        notification.classList.add('show');
        
        // 添加关闭事件
        const closeBtn = notification.querySelector('.notification-close');
        closeBtn.addEventListener('click', () => {
            notification.classList.remove('show');
        });
        
        // 自动关闭
        setTimeout(() => {
            notification.classList.remove('show');
        }, 5000);
    }

    // 高级选项展开/折叠
    const advancedOptionsBtn = document.getElementById('advancedOptionsBtn');
    const advancedOptionsPanel = document.getElementById('advancedOptionsPanel');

    advancedOptionsBtn.addEventListener('click', function() {
        advancedOptionsPanel.classList.toggle('hidden');
        
        if (advancedOptionsPanel.classList.contains('hidden')) {
            advancedOptionsBtn.innerHTML = '<i class="fas fa-cog"></i> 添加更多设置';
        } else {
            advancedOptionsBtn.innerHTML = '<i class="fas fa-times"></i> 收起设置面板';
            
            // 当展开高级设置面板时，加载分类数据（如果尚未加载）
            loadCategories();
        }
    });

    // 加载分类数据的函数
    function loadCategories() {
        const categorySelect = document.getElementById('blogCategory');
        
        // 如果已经加载了分类（除了默认选项外还有其他选项），则不重复加载
        if (categorySelect.options.length > 1) {
            return;
        }
        
        // 显示加载状态
        categorySelect.innerHTML = '<option value="">正在加载分类...</option>';
        
        // 发送请求获取分类数据
        fetch('/api/categories')
            .then(response => {
                if (!response.ok) {
                    throw new Error('获取分类失败');
                }
                return response.json();
            })
            .then(data => {
                // 清空下拉框并添加默认选项
                categorySelect.innerHTML = '<option value="">选择分类</option>';
                
                // 检查响应格式并处理分类数据
                if (data && data.categories && Array.isArray(data.categories)) {
                    // 添加从服务器获取的分类选项
                    data.categories.forEach(category => {
                        const option = document.createElement('option');
                        option.value = category.name; // 使用分类名称作为值
                        
                        // 显示分类名称及文章数量
                        const count = parseInt(category.count) || 0;
                        option.textContent = `${category.name} (${count})`;
                        
                        categorySelect.appendChild(option);
                    });
                    console.log('成功加载分类数据:', data.categories.length, '个分类');
                } else {
                    console.error('服务器返回的分类数据格式不正确:', data);
                    showNotification('分类数据格式不正确', 'error');
                }
            })
            .catch(error => {
                console.error('获取分类失败:', error);
                categorySelect.innerHTML = '<option value="">选择分类</option>';
                showNotification('获取分类失败，请重试', 'warning');
                
                // 添加重试按钮
                const retryOption = document.createElement('option');
                retryOption.value = "_retry_";
                retryOption.textContent = "点击重新加载分类";
                categorySelect.appendChild(retryOption);
                
                // 监听重试选择
                categorySelect.addEventListener('change', function() {
                    if (this.value === "_retry_") {
                        this.value = ""; // 重置为默认选项
                        loadCategories(); // 重新加载
                    }
                });
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

    // 预览模式切换
    const previewBtn = document.getElementById('previewBtn');
    const closePreviewBtn = document.getElementById('closePreview');
    const container = document.querySelector('.main-content .container');
    const previewContent = document.getElementById('previewContent');

    // 预览按钮点击事件
    previewBtn.addEventListener('click', function() {
        // 获取当前编辑器内容
        const title = document.getElementById('blogTitle').value;
        const content = quill.root.innerHTML;
        
        // 更新预览标题和内容
        document.querySelector('.preview-title').textContent = title || '文章预览';
        previewContent.innerHTML = content;
        
        // 移除旧的预览模式样式
        container.classList.remove('preview-mode');
        
        // 清除可能存在的内联样式
        document.querySelectorAll('.main-content .container > *').forEach(el => {
            el.style.width = '';
            el.style.minWidth = '';
            el.style.display = '';
            el.style.flex = '';
        });
        
        // 应用固定的布局
        container.style.display = 'flex';
        container.style.flexDirection = 'row';
        container.style.gap = '20px';
        
        // 设置编辑区域样式
        const editorSection = document.querySelector('.editor-section');
        editorSection.style.width = '50%';
        editorSection.style.minWidth = '300px';
        editorSection.style.flexShrink = '0';
        editorSection.style.overflowY = 'auto';
        editorSection.style.maxHeight = '90vh';
        editorSection.style.position = 'sticky';
        editorSection.style.top = '20px';
        
        // 设置预览区样式
        const previewContainer = document.querySelector('.preview-container');
        previewContainer.style.display = 'block';
        previewContainer.style.flex = '1';
        previewContainer.style.maxHeight = '90vh';
        previewContainer.style.overflowY = 'auto';
        previewContainer.style.position = 'sticky';
        previewContainer.style.top = '20px';
        
        // 隐藏侧边栏
        const sidebar = document.querySelector('.sidebar');
        sidebar.style.display = 'none';
        
        // 处理图片和代码的显示
        formatPreviewContent();
        
        // 在移动设备上添加切换按钮
        if (window.innerWidth <= 992) {
            container.style.flexDirection = 'column';
            editorSection.style.width = '100%';
            
            if (!document.querySelector('.preview-mobile-toggle')) {
                const toggleContainer = document.createElement('div');
                toggleContainer.className = 'preview-mobile-toggle';
                toggleContainer.style.position = 'sticky';
                toggleContainer.style.top = '0';
                toggleContainer.style.zIndex = '100';
                toggleContainer.style.background = 'var(--card-bg)';
                toggleContainer.style.padding = '10px 0';
                toggleContainer.style.marginBottom = '15px';
                toggleContainer.style.boxShadow = 'var(--shadow)';
                
                toggleContainer.innerHTML = `
                    <button type="button" id="showEditor" class="active">编辑</button>
                    <button type="button" id="showPreview">预览</button>
                `;
                container.insertBefore(toggleContainer, container.firstChild);
                
                // 添加移动端切换事件
                document.getElementById('showEditor').addEventListener('click', function() {
                    this.classList.add('active');
                    document.getElementById('showPreview').classList.remove('active');
                    editorSection.style.display = 'block';
                    previewContainer.style.display = 'none';
                });
                
                document.getElementById('showPreview').addEventListener('click', function() {
                    this.classList.add('active');
                    document.getElementById('showEditor').classList.remove('active');
                    editorSection.style.display = 'none';
                    previewContainer.style.display = 'block';
                });
            }
        }
        
        // 添加滚动同步
        setupScrollSync();
    });

    // 关闭预览按钮点击事件
    closePreviewBtn.addEventListener('click', function() {
        // 移除预览模式类和内联样式
        document.querySelectorAll('.main-content .container, .main-content .container > *').forEach(el => {
            el.removeAttribute('style');
        });
        
        // 显示侧边栏
        const sidebar = document.querySelector('.sidebar');
        sidebar.style.display = '';
        
        // 隐藏预览区
        const previewContainer = document.querySelector('.preview-container');
        previewContainer.style.display = 'none';
        
        // 移除移动设备上的切换按钮
        const mobileToggle = document.querySelector('.preview-mobile-toggle');
        if (mobileToggle) {
            mobileToggle.remove();
        }
        
        // 移除滚动同步
        removeScrollSync();
    });

    // 实时预览更新（当编辑器内容变化时）
    quill.on('text-change', function() {
        // 检查预览区域是否可见
        const previewContainer = document.querySelector('.preview-container');
        if (previewContainer && previewContainer.style.display === 'block') {
            const content = quill.root.innerHTML;
            previewContent.innerHTML = content;
            formatPreviewContent();
        }
    });

    // 标题实时预览更新
    document.getElementById('blogTitle').addEventListener('input', function() {
        // 检查预览区域是否可见
        const previewContainer = document.querySelector('.preview-container');
        if (previewContainer && previewContainer.style.display === 'block') {
            const title = this.value;
            document.querySelector('.preview-title').textContent = title || '文章预览';
        }
    });

    // 设置滚动同步
    function setupScrollSync() {
        const editor = document.querySelector('.ql-editor');
        const previewContent = document.getElementById('previewContent');
        let isEditorScrolling = false;
        let isPreviewScrolling = false;

        // 编辑器滚动处理函数
        function editorScrollHandler() {
            if (!isPreviewScrolling) {
                isEditorScrolling = true;
                const scrollPercentage = this.scrollTop / (this.scrollHeight - this.clientHeight);
                const previewScrollTop = scrollPercentage * (previewContent.scrollHeight - previewContent.clientHeight);
                previewContent.scrollTop = previewScrollTop;
                setTimeout(() => {
                    isEditorScrolling = false;
                }, 50);
            }
        }

        // 预览区滚动处理函数
        function previewScrollHandler() {
            if (!isEditorScrolling) {
                isPreviewScrolling = true;
                const scrollPercentage = this.scrollTop / (this.scrollHeight - this.clientHeight);
                const editorScrollTop = scrollPercentage * (editor.scrollHeight - editor.clientHeight);
                editor.scrollTop = editorScrollTop;
                setTimeout(() => {
                    isPreviewScrolling = false;
                }, 50);
            }
        }

        // 添加事件监听器
        editor.addEventListener('scroll', editorScrollHandler);
        previewContent.addEventListener('scroll', previewScrollHandler);

        // 保存事件处理函数引用，用于后续移除
        editor._syncScrollHandler = editorScrollHandler;
        previewContent._syncScrollHandler = previewScrollHandler;
    }

    // 移除滚动同步
    function removeScrollSync() {
        const editor = document.querySelector('.ql-editor');
        const previewContent = document.getElementById('previewContent');

        if (editor && editor._syncScrollHandler) {
            editor.removeEventListener('scroll', editor._syncScrollHandler);
            editor._syncScrollHandler = null;
        }

        if (previewContent && previewContent._syncScrollHandler) {
            previewContent.removeEventListener('scroll', previewContent._syncScrollHandler);
            previewContent._syncScrollHandler = null;
        }
    }

    // 增强预览内容格式化功能
    function formatPreviewContent() {
        // 处理图片的响应式显示
        const images = previewContent.querySelectorAll('img');
        images.forEach(img => {
            img.style.maxWidth = '100%';
            img.style.height = 'auto';
        });
        
        // 处理Quill特殊代码块结构 - 这是关键修复
        const quillCodeBlocks = previewContent.querySelectorAll('.ql-code-block-container');
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
            preElement.style.backgroundColor = '#f6f8fa';
            preElement.style.padding = '16px';
            preElement.style.borderRadius = '6px';
            preElement.style.overflowX = 'auto';
            preElement.style.margin = '1.5em 0';
            preElement.style.maxWidth = '100%';
            preElement.style.boxShadow = '0 2px 8px rgba(0,0,0,0.1)';
            preElement.style.border = '1px solid #e1e4e8';
            
            codeElement.style.fontFamily = 'SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace';
            codeElement.style.fontSize = '0.9em';
            codeElement.style.display = 'block';
            codeElement.style.lineHeight = '1.6';
            codeElement.style.whiteSpace = 'pre'; // 保留空格和换行
            codeElement.style.tabSize = '2';
            codeElement.style.color = '#24292e';
            
            // 设置代码内容，保留换行
            codeElement.textContent = codeText;
            
            // 组装元素
            preElement.appendChild(codeElement);
            
            // 替换原容器
            container.parentNode.replaceChild(preElement, container);
        });
        
        // 继续处理其他可能的代码块形式
        const qlSyntaxBlocks = previewContent.querySelectorAll('.ql-syntax');
        qlSyntaxBlocks.forEach(block => {
            // 跳过已处理的元素
            if (block.closest('.processed')) return;
            
            // 获取原始内容
            const codeText = block.textContent;
            
            // 创建新元素
            const preElement = document.createElement('pre');
            const codeElement = document.createElement('code');
            
            // 设置样式
            preElement.style.backgroundColor = '#f6f8fa';
            preElement.style.padding = '16px';
            preElement.style.borderRadius = '6px';
            preElement.style.overflowX = 'auto';
            preElement.style.margin = '1.5em 0';
            preElement.style.maxWidth = '100%';
            preElement.style.boxShadow = '0 2px 8px rgba(0,0,0,0.1)';
            preElement.style.border = '1px solid #e1e4e8';
            
            codeElement.style.fontFamily = 'SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace';
            codeElement.style.fontSize = '0.9em';
            codeElement.style.display = 'block';
            codeElement.style.lineHeight = '1.6';
            codeElement.style.whiteSpace = 'pre';
            codeElement.style.color = '#24292e';
            
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
        const inlineCodes = previewContent.querySelectorAll('code:not(pre code)');
        inlineCodes.forEach(code => {
            code.style.backgroundColor = '#f1f1f1';
            code.style.padding = '3px 6px';
            code.style.borderRadius = '4px';
            code.style.fontFamily = 'SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace';
            code.style.fontSize = '0.9em';
            code.style.color = '#e83e8c';
            code.style.wordBreak = 'break-word';
        });

        // 处理引用块样式
        const blockquotes = previewContent.querySelectorAll('blockquote');
        blockquotes.forEach(quote => {
            quote.style.borderLeft = '4px solid var(--primary-color, #3498db)';
            quote.style.padding = '1.2em 1.5em';
            quote.style.margin = '1.8em 0';
            quote.style.backgroundColor = 'rgba(52, 152, 219, 0.05)';
            quote.style.color = 'inherit';
            quote.style.borderRadius = '0 8px 8px 0';
            quote.style.boxShadow = '0 2px 8px rgba(0, 0, 0, 0.05)';
            quote.style.position = 'relative';
            
            // 添加引用符号
            if (!quote.querySelector('.quote-mark')) {
                const quoteMark = document.createElement('span');
                quoteMark.className = 'quote-mark';
                quoteMark.textContent = '"';
                quoteMark.style.position = 'absolute';
                quoteMark.style.left = '10px';
                quoteMark.style.top = '-20px';
                quoteMark.style.fontSize = '4em';
                quoteMark.style.opacity = '0.15';
                quoteMark.style.color = 'var(--primary-color, #3498db)';
                quoteMark.style.fontFamily = 'Georgia, serif';
                quoteMark.style.pointerEvents = 'none';
                quote.insertBefore(quoteMark, quote.firstChild);
            }
            
            // 处理引用块内的段落和强调
            const paragraphs = quote.querySelectorAll('p');
            paragraphs.forEach(p => {
                p.style.position = 'relative';
                p.style.zIndex = '1';
                p.style.margin = '0.8em 0';
                p.style.fontStyle = 'italic';
                
                if (p === paragraphs[paragraphs.length - 1]) {
                    p.style.marginBottom = '0';
                }
            });
        });

        // 处理列表样式
        const allLists = previewContent.querySelectorAll('ol, ul');
        allLists.forEach(list => {
            list.style.marginBottom = '1em';
            list.style.paddingLeft = '2em';
            list.style.display = 'block';
            
            // 标记列表类型
            const isOrderedList = list.tagName === 'OL';
            
            // 获取所有列表项
            const listItems = list.querySelectorAll('li');
            listItems.forEach((item, index) => {
                item.style.display = 'list-item';
                item.style.marginBottom = '0.5em';
                
                // 检查data-list属性 - Quill的特性
                const listType = item.getAttribute('data-list');
                
                // 设置列表样式
                if (listType === 'bullet') {
                    item.style.listStyleType = 'disc';
                } else if (listType === 'ordered') {
                    item.style.listStyleType = 'decimal';
                    // 如果是有序列表项，设置value属性
                    item.setAttribute('value', index + 1);
                } else {
                    // 根据容器类型设置默认样式
                    if (isOrderedList) {
                        item.style.listStyleType = 'decimal';
                        item.setAttribute('value', index + 1);
                    } else {
                        item.style.listStyleType = 'disc';
                    }
                }
                
                // 确保没有float和display:inline干扰显示
                item.style.float = 'none';
                item.style.display = 'list-item';
            });
        });

        // 移除Quill UI元素
        const qlUiElements = previewContent.querySelectorAll('.ql-ui');
        qlUiElements.forEach(element => {
            element.style.display = 'none';
        });

        // 处理标题样式
        const headings = previewContent.querySelectorAll('h1, h2, h3, h4, h5, h6');
        headings.forEach(heading => {
            heading.style.marginTop = '1.5em';
            heading.style.marginBottom = '0.8em';
            heading.style.fontWeight = '600';
            heading.style.lineHeight = '1.4';
            heading.style.color = 'var(--text-primary, #333)';
        });

        // 处理段落样式
        const paragraphs = previewContent.querySelectorAll('p:not(blockquote p)');
        paragraphs.forEach(p => {
            p.style.marginBottom = '1em';
            p.style.lineHeight = '1.8';
        });
    }
});