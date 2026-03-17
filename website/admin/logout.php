<?php
require_once 'auth.php';
do_logout();
header('Location: login.php');
exit;
