// floating_dropdown.js
document.addEventListener('DOMContentLoaded', function() {
    const dropdown = document.querySelector('.floating-dropdown');
    const header = dropdown.querySelector('.floating-dropdown-header');
    const toggleBtn = dropdown.querySelector('.toggle-btn');
    let isDragging = false;
    let currentX;
    let currentY;
    let initialX;
    let initialY;
    let xOffset = 0;
    let yOffset = 0;
    let startPos = { x: 0, y: 0 }; // 记录点击开始位置

    // 开始拖动
    function startDragging(e) {
        // 记录起始点击位置
        startPos = {
            x: e.type === "touchstart" ? e.touches[0].clientX : e.clientX,
            y: e.type === "touchstart" ? e.touches[0].clientY : e.clientY
        };

        if (e.type === "touchstart") {
            initialX = e.touches[0].clientX - xOffset;
            initialY = e.touches[0].clientY - yOffset;
        } else {
            initialX = e.clientX - xOffset;
            initialY = e.clientY - yOffset;
        }

        if (e.target === header || e.target.parentElement === header) {
            isDragging = true;
            dropdown.classList.add('dragging');
        }
    }

    // 处理拖动
    function handleDrag(e) {
        if (isDragging) {
            e.preventDefault();
            if (e.type === "touchmove") {
                currentX = e.touches[0].clientX - initialX;
                currentY = e.touches[0].clientY - initialY;
            } else {
                currentX = e.clientX - initialX;
                currentY = e.clientY - initialY;
            }
            xOffset = currentX;
            yOffset = currentY;
            setTranslate(currentX, currentY, dropdown);
        }
    }

    // 结束拖动
    function stopDragging(e) {
        // 计算移动距离
        const endPos = {
            x: e.type === "touchend" ? e.changedTouches[0].clientX : e.clientX,
            y: e.type === "touchend" ? e.changedTouches[0].clientY : e.clientY
        };
        
        const moveDistance = Math.sqrt(
            Math.pow(endPos.x - startPos.x, 2) + 
            Math.pow(endPos.y - startPos.y, 2)
        );

        isDragging = false;
        dropdown.classList.remove('dragging');

        // 如果移动距离小于5px，视为点击而不是拖动
        if (moveDistance < 5) {
            toggleDropdown(e);
        }
    }

    // 设置位置
    function setTranslate(xPos, yPos, el) {
        el.style.transform = `translate(${xPos}px, ${yPos}px)`;
    }

    // 切换下拉框显示状态
    function toggleDropdown(e) {
        // 阻止事件冒泡，这样点击下拉框内部时不会触发document的点击事件
        e.stopPropagation();
        dropdown.classList.toggle('active');
        toggleBtn.textContent = dropdown.classList.contains('active') ? '▲' : '▼';
    }

    // 点击页面其他地方关闭下拉框
    function closeDropdownOutside(e) {
        if (!dropdown.contains(e.target)) {
            dropdown.classList.remove('active');
            toggleBtn.textContent = '▼';
        }
    }

    // 添加事件监听
    header.addEventListener('mousedown', startDragging);
    header.addEventListener('touchstart', startDragging);

    document.addEventListener('mousemove', handleDrag);
    document.addEventListener('touchmove', handleDrag, { passive: false });

    document.addEventListener('mouseup', stopDragging);
    document.addEventListener('touchend', stopDragging);

    // 点击其他区域关闭下拉框
    document.addEventListener('click', closeDropdownOutside);
});