import React from 'react';
import{Link} from 'react-router-dom'
import styles from './Navbar.module.css'
import logo from '../blue/logo.png';
import logo_toi from '../blue/logo_toi.png';

function Navbar(){

return(
 
<nav className={styles.navbar}>
<ul className={styles.list}>
    <li>
    <p>
    <img src={logo} alt="Logo"/> 
    <img src={logo_toi} alt="Logo"/>
    </p>
    </li>
        

        <li className={styles.item}> 
            <Link to="/">Home</Link>
        </li>
        <li className={styles.item}>
            <Link to="/Energy">Energy</Link>
        </li>
        <li className={styles.item}>
            <Link to="/Graphics">Graphics</Link>
        </li>
        <li className={styles.item}>
            <Link to="/RealTime">RealTime</Link>
        </li>
        <li className={styles.item}>
            <Link to= "/MqttReact" > MqttReact </Link>
        </li>
        <li className={styles.item}>
            <Link to= "/Dashboard" > Dashboard </Link>
        </li>
        <li className={styles.item}>
            <Link to= "/Schedule"> Schedule</Link>
        </li>
        <li className={styles.item}>
            <Link to= "/RobotMonitor"> RobotMonitor</Link>
        </li>
       <li className={styles.item}>
            <Link to= "/Ecar"> Ecar</Link>
        </li>
        <li className={styles.item}>
            <Link to= "/Users"> Users</Link>
        </li>
        <li className={styles.item}>
            <Link to= "/BotPress"> Bot</Link>
        </li>
    </ul>
   </nav>
)
}
export default Navbar
