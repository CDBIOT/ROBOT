
import React, { useEffect } from 'react';
import {BrowserRouter as Router, Route , Routes} from 'react-router-dom';
import Navbar from './Components/Navbar';

import Home from './Components/pages/Home';
import Schedule from './Components/pages/Schedule';
import Users from './Components/pages/Users';
import Graphics from './Components/pages/Graphics';
import RealTime from './Components/pages/RealTime'
import Energy from './Components/pages/Energy';
import MqttReact from './Components/pages/MqttReact';
import Dashboard from './Components/pages/Dashboard';
import RobotMonitor from './Components/pages/RobotMonitor';
import Ecar from './Components/pages/Ecar';
import BotPress from './Components/pages/BotPress';


function App() {

return (
  <div className = "App" > 
 <Router>
    <Navbar />
   
         <Routes>
            <Route exact path="/"   element={<Home />}></Route>
            <Route path="/Energy"   element={<Energy />}></Route>
            <Route path='/Graphics' element={<Graphics />}></Route>
            <Route path="/Schedule" element={<Schedule />}></Route>
            <Route path="/RobotMonitor" element={<RobotMonitor />}></Route>
            <Route path="/Ecar" element={<Ecar />}></Route>
            <Route path="/Users"    element={<Users />} ></Route>
            <Route path='/RealTime' element={<RealTime />}></Route>
            <Route path='/MqttReact' element={<MqttReact />}></Route> 
            <Route path='/Dashboard' element={<Dashboard />}></Route> 
            <Route path='/BotPress' element={<BotPress />}></Route> 
         </Routes>
    </Router>
  </div>
    );
}

export default(App);

