// script.js
document.addEventListener('DOMContentLoaded', function() {
    // 获取所有导航项和页面
    const navItems = document.querySelectorAll('.nav-links li');
    const pages = document.querySelectorAll('.page');

    // 为每个导航项添加点击事件
    navItems.forEach(item => {
        item.addEventListener('click', function() {
            // 移除所有导航项的active类
            navItems.forEach(nav => nav.classList.remove('active'));
            // 为当前点击的导航项添加active类
            this.classList.add('active');

            // 获取对应的页面ID
            const pageId = this.getAttribute('data-page');
            
            // 隐藏所有页面
            pages.forEach(page => page.classList.remove('active'));
            // 显示当前选中的页面
            document.getElementById(pageId).classList.add('active');
        });
    });
});