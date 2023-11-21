// Import required modules
const express = require('express');
const multer = require('multer');
const path = require('path');

const app = express();
const port = process.env.PORT || 3000;

const storage = multer.diskStorage({
  destination: (req, file, cb) => {
    cb(null, 'uploads/');
  },
  filename: (req, file, cb) => {
    const ext = path.extname(file.originalname);
    cb(null, 'uploaded_image' + ext);
  },
});

const upload = multer({ storage });

app.post('/v1/api/images/upload', upload.single('imageFile'), (req, res) => {
  if (req.file) {
    res.status(200).json({ message: 'Image uploaded successfully' });
  } else {
    res.status(400).json({ error: 'No image uploaded' });
  }
});

app.listen(port, () => {
  console.log(`Server is running on port ${port}`);
});
