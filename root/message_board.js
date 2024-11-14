let comments = [];

function postComment() {
  const commentInput = document.getElementById("comment-input");
  const commentText = commentInput.value.trim();
  if (commentText) {
    fetch("/comments", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ text: commentText, timestamp: new Date().toLocaleString() }),
    })
      .then((response) => response.json())
      .then((comment) => {
        comments.push(comment);
        renderComments();
        commentInput.value = "";
      })
      .catch((error) => console.error("Error posting comment:", error));
  }
}

function getComments() {
  fetch("/comments")
    .then((response) => response.json())
    .then((commentsData) => {
      comments = commentsData;
      renderComments();
    })
    .catch((error) => console.error("Error fetching comments:", error));
}

// 这个函数用于将留言渲染到留言板上面
function renderComments() {
  const commentList = document.querySelector(".comment-list");
  commentList.innerHTML = "";
  comments.forEach((comment) => {
    const commentElement = document.createElement("div");
    commentElement.classList.add("comment");
    commentElement.innerHTML = `
      <p>${comment.text}</p>
      <p><small>发表于 ${comment.timestamp}</small></p>
    `;
    commentList.appendChild(commentElement);
  });
}

// 在页面加载时获取初始留言列表
getComments();