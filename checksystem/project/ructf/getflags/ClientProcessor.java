package ructf.getflags;

import java.net.*;
import java.sql.Connection;
import java.sql.SQLException;
import java.util.*;

import org.apache.log4j.*;

import ructf.main.*;

public class ClientProcessor extends Thread
{
	private Socket				clientSock;
	private LoggingWriter		out;
	private LoggingReader		in;
	private Connection			dbConnection;
	private FlagManager			flagManager;
	private TimeoutCloser		timeoutCloser;
	
	public static Logger		logger;
	public static Logger		loggerDump;
	
	private static String		clientBanner = "  ..:: RuCTFE Get-Flags service ::..";
	
	private static Map<InetAddress, Long> lastAccess = new HashMap<InetAddress, Long>();
	
	public static void CreateAndStart(Socket clientSock)
	{
		try {
			new ClientProcessor(clientSock);
		}
		catch (Exception e) {
			logger.error("Problem with: " + clientSock);
			Main.logException(e);
		}
	}
	
	private static void updateLastAccessTime(InetAddress addr) {
		synchronized (lastAccess) {
			lastAccess.put(addr, System.currentTimeMillis()); 
		}
	}
	
	private static boolean isTooFast(InetAddress addr) {
		synchronized (lastAccess) {
			if (!lastAccess.containsKey(addr))
				return false;
			if (System.currentTimeMillis() - lastAccess.get(addr) < Constants.getFlagsMinReconnectTime*1000)
				return true;
			return false;
		}
	}
	
	private ClientProcessor(Socket clientSock) throws Exception
	{
		InetAddress addr = clientSock.getInetAddress();
		logger.info("Connect: " + addr.getHostAddress());
		
		this.clientSock = clientSock;
		this.out = LoggingWriter.Create(clientSock, loggerDump);

		boolean tooFast = isTooFast(addr);
		updateLastAccessTime(addr);
		if (tooFast) {
			logger.warn("Too fast reconnect: " + addr.getHostAddress());
			out.println("Too fast reconnect: " + addr.getHostAddress());
			out.close();
			clientSock.close();
			return;
		}
		
		this.in = LoggingReader.Create(clientSock, loggerDump);
		this.timeoutCloser = new TimeoutCloser(clientSock, out, Constants.getFlagsClientTimeout);
		this.dbConnection = DatabaseManager.CreateConnection();
		this.flagManager = new FlagManager(dbConnection);
		start();
	}
	
	public void run()
	{
		NDC.push(clientSock.getInetAddress().getHostAddress());
		int goodFlags = 0, totalFlags = 0;
		
		try {
			out.println(clientBanner);
			out.println("");
			
			TeamId teamId = new TeamId(clientSock.getInetAddress(), dbConnection);
			out.println("Your team id is " + teamId.getId() );
			out.println("Enter your flags, finished with newline (or empty line to exit)");
			
			while (true)
			{
//				if (totalFlags - goodFlags >= Constants.getFlagsDisconnectThreshold) {
//					out.println("Too many bad flags!");
//					break;
//				}
//				out.print("> ");
				
				String flagStr = in.readLine();
				timeoutCloser.resetTimer();
				if (flagStr==null || flagStr.length() == 0) {
					out.println("Goodbye!");
					break;
				}
				totalFlags++;
			
				
				if (flagStr.length() != Constants.flagLength) {
					out.println("Denied: bad flag length. Must be: " + Constants.flagLength);
//					logger.debug("Denied: bad length");
					continue;
				}
				logger.debug(String.format("Got flag: %s (%d)", flagStr, totalFlags));
				
				
				
//				try{
//					flagManager.InsertTaskStolenFlag(teamId.getId(), flagStr);
//					goodFlags++;
//					logger.debug(String.format("Accepted taskFlag %s", flagStr));
//					out.println("Accepted");
//					continue;
//				}
//				catch(NoSuchTaskFlagException ne){
//					logger.debug("Denied: no such task flag");
//					out.println("Denied: no such flag");
//					continue;
//				}
//				catch (DuplicateTaskFlagForTeam de) {
//					logger.debug("Denied: task flag resubmit");
//					out.println("Denied: you already submitted this flag");
//					continue;
//				}
//				catch (SQLException e) {
//					logger.error("InsertTaskStolenFlag failed", e);
//					out.println("Please try again later");
//				}
				
				
				
				StolenFlag stolenFlag = new StolenFlag(flagStr, dbConnection);
				if (stolenFlag.noSuchFlag()) {
					logger.debug("Denied: no such flag");
					out.println("Denied: no such flag");					
					continue;
				}
				
				if (stolenFlag.getOwnerTeamId() == teamId.getId()) {
					logger.debug("Denied: own flag");
					out.println("Denied: flag is your own");					
					continue;
				}
				
				if (stolenFlag.wasStolenByTeam(teamId.getId())) {
					logger.debug("Denied: resubmit");
					out.println("Denied: you already submitted this flag");					
					continue;
				}
				
				if (stolenFlag.getAgeSeconds() >= Constants.flagExpireInterval ) {
					logger.debug("Denied: too old");
					out.println("Denied: flag is too old");					
					continue;
				}
				
				int serviceId = stolenFlag.getServiceId();
				ServiceStatus serviceStatus = new ServiceStatus(teamId.getId(), serviceId, dbConnection);
				if (serviceStatus.getStatus() != CheckerExitCode.OK.toInt()) {
					String servName = serviceStatus.getServiceName();
					logger.debug("Denied: service not up: " + servName);
					out.println(String.format("Denied: your appropriate service (%s) is not UP", servName));					
					continue;
				}
				
				if (flagManager.InsertStolenFlag(teamId.getId(), stolenFlag))
				{
					goodFlags++;
					logger.debug(String.format("Accepted flag %s", flagStr));
					out.println("Accepted");
				}
				else {
					logger.error("InsertStolenFlag failed");
					out.println("Please try again later");
				}
			}
		}
		catch (UnknownAddressException e) {
			logger.warn("Unknown ip: " + e.getAddress());
			out.println(String.format("Your IP address (%s) is unknown", e.getAddress()));
		}
		catch (SocketException e) { 
			logger.warn("SocketException: " + e.getMessage());
		} 
		catch(Exception e) {
			logger.error("Unexpected exception: " + e.getMessage());
			Main.logException(e);
			out.println("Please try again later");
		}
		finally {
			FreeResources();
		}
	}
	
	private void FreeResources()
	{
		logger.info("Disconnect");
		updateLastAccessTime(clientSock.getInetAddress());
		try {
			timeoutCloser.exit();
			dbConnection.close();
			in.close();
			out.close();
			clientSock.close();
		} catch (Exception e) {
			logger.warn("FreeResources:");
			Main.logException(e);
		}
	}
}
